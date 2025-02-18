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
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"

#include "fdutil.h"
#include "timeutil.h"
// #include "debughelper.h"


int LiteWebServer::exit_event_ = -1;
void LiteWebServer::handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        uint64_t event_count = 1;
        // 不管返回值，失败就是失败算了，
        // 如果失败了，就是无法按正常流程退出
        write(LiteWebServer::exit_event_, &event_count, sizeof(event_count));
    }
}

LiteWebServer::LiteWebServer(const ServerConf srv_conf)
    : srv_conf_(srv_conf)
    , running_(false)
    , epoll_fd_(-1)
    , srv_sock_(-1)
    , eventpool_(srv_conf_.nthread_)
    , events_(new struct epoll_event[srv_conf_.epoll_max_events_])
    , pool_idx_(0)
{
    init_log();

    // 当服务端在send等操作时，客户端突然断开连接，
    // 会触发SIGPIPE信号，导致进程退出
    // 这里忽略SIGPIPE信号，避免进程退出
    ignore_SIGPIPE();
    register_exit_signal();
    FdUtil::set_nonblocking(exit_event_);

    create_listen_service();
    FdUtil::set_nonblocking(srv_sock_);

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error(strerror(errno));
    }

    uint32_t srv_sock_events = EPOLLIN | EPOLLRDHUP;
    if (srv_conf_.epoll_et_srv_) {
        srv_sock_events |= EPOLLET;
    }
    FdUtil::epoll_add_fd(epoll_fd_, srv_sock_, srv_sock_events);
    FdUtil::epoll_add_fd_oneshot(epoll_fd_, exit_event_, EPOLLIN | EPOLLRDHUP);

    //BUG 如何优雅的结束程序？如何关闭sock？如何释放资源？
    for (int i = 0; i < srv_conf_.nthread_; ++i) {
        conn_loops_.emplace_back(std::make_shared<ConnLoop>(&srv_conf_));
        eventpool_.enqueue(&ConnLoop::loop, conn_loops_[i]);
    }
}

LiteWebServer::~LiteWebServer()
{
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }

    if (srv_sock_ >= 0) {
        close(srv_sock_);
    }

    if (exit_event_ >= 0) {
        close(exit_event_);
    }

    if (events_) {
        delete[] events_;
    }
}

void LiteWebServer::start_loop()
{
    SPDLOG_INFO("START server main loop at {}...", system_nowtime_str(true));

    running_ = true;
    int n_event = 0;

    while(running_) {
        n_event = epoll_wait(epoll_fd_, events_, srv_conf_.epoll_max_events_, -1);

        // 如果等待事件失败，且不是因为系统中断造成的，
        // 直接退出主循环
        if (n_event < 0 && errno != EINTR) {
            SPDLOG_ERROR("Main server loop epoll_wait error: {}", strerror(errno));
            running_ = false;
            break;
        }

        for (int i = 0; i < n_event; ++i) {
            int sockfd = events_[i].data.fd;
            // unsigned int events_n = events_[i].events;
            // std::string events_s = events_to_str(events_[i].events);
            // SPDLOG_DEBUG("epoll event: sockfd: {}, events_n: {}, events_s: {}", sockfd,  events_n, events_s); 

            // 处理新的连接
            if (sockfd == srv_sock_) {
                deal_new_conn_greedy();
            } else if (sockfd == exit_event_) {
                SPDLOG_DEBUG("Main server loop recive exit signal");
                running_ = false;
                break;
            }
        }
    }

    // 停掉所有事件循环
    // 这里只要简单的调用stop就行，
    // eventpool_线程池在销毁时会等待所有线程退出
    for (int i = 0; i < srv_conf_.nthread_; ++i) {
        conn_loops_[i]->stop();
    }

    SPDLOG_INFO("END server main loop at {}...", system_nowtime_str(true));
}

void LiteWebServer::init_log()
{
    // output to console
    // try {
    //     // Auto flush when "trace" or higher message is logged on all loggers
    //     spdlog::flush_on(spdlog::level::trace);
    //     // Custom pattern
    //     spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P|%t] [%l] %v");
    //     spdlog::set_level(spdlog::level::debug);
    // } catch (const spdlog::spdlog_ex &ex) {
    //     std::cout << "Log initialization failed: " << ex.what() << std::endl;
    // }

    // output to file with no rotation and multithread
    // #include "spdlog/sinks/basic_file_sink.h"
    // try {
    //     // Auto flush when "trace" or higher message is logged on all loggers
    //     spdlog::flush_on(spdlog::level::debug);
    //     // Custom pattern
    //     spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P|%t] [%l] %v");
    //     spdlog::set_level(spdlog::level::debug);
    //     spdlog::basic_logger_mt("server_log", "litewebserver.log");
    //     spdlog::set_default_logger(spdlog::get("server_log"));
    // } catch (const spdlog::spdlog_ex &ex) {
    //     std::cout << "Log initialization failed: " << ex.what() << std::endl;
    // }

    // ASYNCHRONOUS
    // output to file with no rotation and multithread
    // #include "spdlog/sinks/basic_file_sink.h"
    // #include "spdlog/async.h"
    try {
        // Auto flush when "trace" or higher message is logged on all loggers
        spdlog::flush_on(spdlog::level::info);
        // Custom pattern
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P|%t] [%l] %v");
        //!!! 为了提高日志效率，日志等级在编译时期被禁用
        // 修改set_level的同时需要修改spdlog/tweakme.h下的宏
        // #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
        spdlog::set_level(spdlog::level::info);
        spdlog::basic_logger_mt<spdlog::async_factory>("server_log", "litewebserver.log");
        spdlog::set_default_logger(spdlog::get("server_log"));
    } catch (const spdlog::spdlog_ex &ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void LiteWebServer::create_listen_service()
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

    exit_event_ = eventfd(0, 0);
    if (exit_event_ == -1) {
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
    FdUtil::set_nonblocking(cli_sock);
    // FdUtil::set_socket_nodelay(cli_sock);
    //BUG 优化分配方式
    // 简单的分配接收到的链接，这种方式有一个缺陷，
    // 如果连接存在部分长连接，部分短连接，会导致分配不均
    conn_loops_[pool_idx_]->add_clisock_to_queue(cli_sock);
    pool_idx_ = (pool_idx_ + 1) % srv_conf_.nthread_;

    // SPDLOG_DEBUG("Recive client connect, cli_sock: {}, ip: {}",
    //              cli_sock, inet_ntoa(cli_addr.sin_addr));
}

void LiteWebServer::deal_new_conn_greedy()
{
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_size = sizeof(cli_addr);
    //TODO 仅初始化一次，应该是没问题的
    memset(&cli_addr, 0, cli_addr_size);

    while (true) {
        int cli_sock = accept(srv_sock_, (struct sockaddr *)&cli_addr, &cli_addr_size);
        if (cli_sock < 0) {
            // 除了系统中断外，都应该直接退出，目前是这样的
            if (errno == EINTR) { continue; }
            else {
                SPDLOG_ERROR("deal_new_conn_greedy accept error: {}", strerror(errno));
                break;
            }
        }

        FdUtil::set_nonblocking(cli_sock);
        // FdUtil::set_socket_nodelay(cli_sock);
        //BUG 优化分配方式
        // 简单的分配接收到的链接，这种方式有一个缺陷，
        // 如果连接存在部分长连接，部分短连接，会导致分配不均
        conn_loops_[pool_idx_]->add_clisock_to_queue(cli_sock);
        pool_idx_ = (pool_idx_ + 1) % srv_conf_.nthread_;
        // SPDLOG_DEBUG("Recive client connect, cli_sock: {}, ip: {}",
        //              cli_sock, inet_ntoa(cli_addr.sin_addr));
    }
}
