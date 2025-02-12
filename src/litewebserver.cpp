#include "litewebserver.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <functional>

#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "spdlog/spdlog.h"

#include "fdutil.h"
#include "debughelper.h"

int LiteWebServer::exit_event = eventfd(0, 0);
void LiteWebServer::handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        uint64_t event_count = 1;
        // 不管返回值，失败就是失败算了，
        // 如果失败了，就是无法按正常流程退出
        write(LiteWebServer::exit_event, &event_count, sizeof(event_count));
    }
}

LiteWebServer::LiteWebServer(const ServerConf srv_conf)
    : srv_conf_(srv_conf)
    , running_(false)
    , epoll_fd_(-1)
    , srv_sock_(-1)
    , workerpool_(chaos::ThreadPool(srv_conf_.nthread_))
    , events_(new struct epoll_event[srv_conf_.epoll_max_events_])
    , conns_(10000)  // 预分配大小，降低哈希开销，空间换时间
{
    // 预分配一定的空间，避免频繁扩容
    expired_.reserve(1000);

    init_log();

    // 当服务端在send等操作时，客户端突然断开连接，
    // 会触发SIGPIPE信号，导致进程退出
    // 这里忽略SIGPIPE信号，避免进程退出
    ignore_SIGPIPE();
    register_exit_signal();
    FdUtil::set_nonblocking(exit_event);

    create_listen_serve();
    FdUtil::set_nonblocking(srv_sock_);

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error(strerror(errno));
    }

    FdUtil::epoll_add_fd(epoll_fd_, srv_sock_, EPOLLIN | EPOLLRDHUP);
    FdUtil::epoll_add_fd(epoll_fd_, exit_event, EPOLLIN);
}

LiteWebServer::~LiteWebServer()
{
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }

    if (srv_sock_ >= 0) {
        close(srv_sock_);
    }

    if (events_) {
        delete[] events_;
    }
}

bool LiteWebServer::is_running()
{
    return running_;
}

void LiteWebServer::start_loop()
{
    // SPDLOG_DEBUG("Start server loop...");
    running_ = true;
    int n_event = 0;

    while(running_) {
        n_event = epoll_wait(epoll_fd_, events_, srv_conf_.epoll_max_events_, DEF_TIMEOUT_S);

        // 如果等待事件失败，且不是因为系统中断造成的，
        // 直接退出主循环
        if (n_event < 0 && errno != EINTR) {
            // SPDLOG_ERROR("epoll_wait error: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < n_event; ++i) {
            int sockfd = events_[i].data.fd;
            // unsigned int events_n = events_[i].events;
            // std::string events_s = events_to_str(events_[i].events);
            // SPDLOG_DEBUG("epoll event: sockfd: {}, events_n: {}, events_s: {}", sockfd,  events_n, events_s); 

            if (sockfd == exit_event) {
                // SPDLOG_DEBUG("Recive exit signal");
                running_ = false;
                break;
            }

            // 处理新的连接
            if (sockfd == srv_sock_) {
                deal_new_conn();
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // SPDLOG_DEBUG("{} Recv EPOLLRDHUP | EPOLLHUP | EPOLLERR", sockfd);
                disconn_one(events_[i].data.fd);
            } else if (events_[i].events & EPOLLIN) {
                deal_conn_in(sockfd);
            } else if (events_[i].events & EPOLLOUT) {
                deal_conn_out(sockfd);
            }
        }
        
        //TODO
        // 将this 传给timer_mgr_，在超时时就处理链接，
        // 省去一次循环，看看性能能提升多少，不过耦合性会提升
        expired_.clear();
        timer_mgr_.handle_expired_timers(expired_);
        for (auto &id : expired_) {
            deal_conn_expired(id);
        }
    }
}

void LiteWebServer::init_log()
{
    try {
        // Auto flush when "trace" or higher message is logged on all loggers
        spdlog::flush_on(spdlog::level::trace);
        // Custom pattern
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P|%t] [%l] %v");
        spdlog::set_level(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex &ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void LiteWebServer::create_listen_serve()
{
    int sock_ret = -1;

    srv_sock_ = socket(PF_INET, SOCK_STREAM, 0);
    if (srv_sock_ < 0) {
        throw std::runtime_error(strerror(errno));
    }

    sock_ret = FdUtil::set_socket_reuseaddr(srv_sock_);
    if (sock_ret == -1) {
        throw std::runtime_error(strerror(errno));
    }

    struct sockaddr_in host_addr;
    socklen_t host_addr_size = sizeof(host_addr);
    memset(&host_addr, 0, host_addr_size);
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    host_addr.sin_port = htons(srv_conf_.port_);
    sock_ret = bind(srv_sock_, (struct sockaddr*)&host_addr, host_addr_size);

    sock_ret = listen(srv_sock_, srv_conf_.listen_queue_n_);
    if (sock_ret == -1) {
        throw std::runtime_error(strerror(errno));
    }
}

void LiteWebServer::register_exit_signal()
{
    int ret = -1;

    if (exit_event == -1) {
        throw std::runtime_error(strerror(errno));
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &LiteWebServer::handle_signal;
    sigfillset(&sa.sa_mask);
    
    ret = sigaction(SIGTERM, &sa, NULL);
    if (ret == -1) {
        throw std::runtime_error(strerror(errno));
    }

    ret = sigaction(SIGINT, &sa, NULL);
    if (ret == -1) {
        throw std::runtime_error(strerror(errno));
    }
}

void LiteWebServer::ignore_SIGPIPE()
{
    int ret = -1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigfillset(&sa.sa_mask);

    ret = sigaction(SIGPIPE, &sa, NULL);
    if (ret == -1) {
        throw std::runtime_error(strerror(errno));
    }
}

void LiteWebServer::deal_new_conn()
{
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_size = sizeof(cli_addr);
    memset(&cli_addr, 0, cli_addr_size);

    int cli_sock = accept(srv_sock_, (struct sockaddr *)&cli_addr, &cli_addr_size);
    if (cli_sock < 0) {
        return;
    }
    conns_.emplace(cli_sock, std::make_shared<UserConn>(this, &srv_conf_, cli_sock));

    FdUtil::set_nonblocking(cli_sock);
    // FdUtil::set_socket_nodelay(cli_sock);
    // 通过设置EPOLLONESHOT保证: 每个UserConn同一时间在线程池中只有一个任务
    // 避免了潜在的竟态问题，降低复杂性
    // 不清楚有没有不用EPOLLONESHOT的设计方案
    FdUtil::epoll_add_fd_oneshot(epoll_fd_, cli_sock, EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR);
    // SPDLOG_DEBUG("Recive client connect, cli_sock: {}, ip: {}",
    //              cli_sock, inet_ntoa(cli_addr.sin_addr));

    timer_mgr_.add_timer(cli_sock);
}

void LiteWebServer::disconn_one(int cli_sock)
{
    conns_.erase(cli_sock);
    timer_mgr_.rm_timer(cli_sock);
    FdUtil::epoll_del_fd(epoll_fd_, cli_sock);
    close(cli_sock);
    // SPDLOG_DEBUG("Disconnect client, cli_sock: {}", cli_sock);
}

void LiteWebServer::deal_conn_in(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }
    one_user->second->set_state(UserConn::ConnState::CONN_DEALING);
    timer_mgr_.add_timer(cli_sock);
    workerpool_.enqueue(UserConn::static_process_in, one_user->second);
    // SPDLOG_DEBUG("deal conn in, add to work queue, cli_sock: {}", cli_sock);
}

void LiteWebServer::deal_conn_out(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }
    one_user->second->set_state(UserConn::ConnState::CONN_DEALING);
    timer_mgr_.add_timer(cli_sock);
    workerpool_.enqueue(UserConn::static_process_out, one_user->second);
}

void LiteWebServer::deal_conn_expired(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }

    UserConn::ConnState state = one_user->second->get_state();
    if (state == UserConn::ConnState::CONN_DEALING) {
        // SPDLOG_DEBUG("Conn {} still dealing add timer", cli_sock);
        timer_mgr_.add_timer(cli_sock);
    } else {
        // SPDLOG_DEBUG("Conn {} do disconnect", cli_sock);
        disconn_one(cli_sock);
    }
}

void LiteWebServer::modify_conn_event_read(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epoll_fd_, cli_sock, EPOLLIN | EPOLLRDHUP);
}

void LiteWebServer::modify_conn_event_write(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epoll_fd_, cli_sock, EPOLLOUT | EPOLLRDHUP);
}

void LiteWebServer::modify_conn_event_close(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epoll_fd_, cli_sock, EPOLLRDHUP);
}
