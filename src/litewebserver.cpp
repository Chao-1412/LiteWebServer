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

#include "spdlog/spdlog.h"

#include "fdutil.h"
#include "debughelper.h"


void LiteWebServer::handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
    }
}

LiteWebServer::LiteWebServer(const ServerConf srv_conf)
    : srv_conf_(srv_conf)
    , running_(false)
    , epoll_fd_(-1)
    , srv_sock_(-1)
    , workerpool_(chaos::ThreadPool(srv_conf_.nthread_))
    , events_(new struct epoll_event[srv_conf_.epoll_max_events_])
    , conns_(5000)  // 预分配大小，降低哈希开销，空间换时间
{
    init_log();

    create_listen_serve();

    FdUtil::set_nonblocking(srv_sock_);

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error(strerror(errno));
    }

    FdUtil::epoll_add_fd(epoll_fd_, srv_sock_, EPOLLIN | EPOLLRDHUP);
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
    running_ = true;
    int n_event = 0;

    while(running_) {
        n_event = epoll_wait(epoll_fd_, events_, srv_conf_.epoll_max_events_, -1);

        // 如果等待事件失败，且不是因为系统中断造成的，
        // 直接退出主循环
        if (n_event < 0 && errno != EINTR) {
            SPDLOG_INFO("epoll_wait error: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < n_event; ++i) {
            int sockfd = events_[i].data.fd;

            // 处理新的连接
            if (sockfd == srv_sock_) {
                deal_new_conn();
            } else if (events_[i].events & EPOLLRDHUP) {
                disconn_one(events_[i].data.fd);
            } else if (events_[i].events & EPOLLIN) {
                deal_conn_in(sockfd);
            } else if (events_[i].events & EPOLLOUT) {
                deal_conn_out(sockfd);
            }
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

    int sockopt_reuse = 1;
    sock_ret = setsockopt(srv_sock_, SOL_SOCKET, SO_REUSEADDR, &sockopt_reuse, sizeof(sockopt_reuse));
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

void LiteWebServer::register_signal()
{
    int ret = -1;

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
    // 通过设置EPOLLONESHOT保证，每个UserConn同一时间在线程池中只有一个任务
    // 避免了潜在的竟态问题，降低复杂性
    // 不清楚有没有不用EPOLLONESHOT的设计方案
    FdUtil::epoll_add_fd_oneshot(epoll_fd_, cli_sock, EPOLLIN | EPOLLRDHUP);
}

void LiteWebServer::disconn_one(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }

    one_user->second->close_conn();
    conns_.erase(cli_sock);
    FdUtil::epoll_del_fd(epoll_fd_, cli_sock);
    close(cli_sock);
}

void LiteWebServer::deal_conn_in(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }
    workerpool_.enqueue(UserConn::static_process_in, one_user->second);
}

void LiteWebServer::deal_conn_out(int cli_sock)
{
    auto one_user = conns_.find(cli_sock);
    if (one_user == conns_.end()) {
        return;
    }
    workerpool_.enqueue(UserConn::static_process_out, one_user->second);
}

void LiteWebServer::modify_conn_event_read(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epoll_fd_, cli_sock, EPOLLIN | EPOLLRDHUP);
}

void LiteWebServer::modify_conn_event_write(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epoll_fd_, cli_sock, EPOLLOUT | EPOLLRDHUP);
}
