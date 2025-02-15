#include "connloop.h"

#include <stdexcept>
#include <tuple>
#include <utility>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#include "fdutil.h"


ConnLoop::ConnLoop(const ServerConf *const srv_conf, int epoll_wait_timeout)
    : srv_conf_(srv_conf)
    , running_(false)
    , epoll_wait_timeout_(epoll_wait_timeout)
    , epfd_(epoll_create1(EPOLL_CLOEXEC))
    , events_(new struct epoll_event[srv_conf_->epoll_max_events_])
    , conns_(10000)
{
    expired_.reserve(10000);
    if (epfd_ == -1) { throw std::runtime_error(strerror(errno)); }
}

ConnLoop::~ConnLoop()
{
    if (epfd_ >= 0) { close(epfd_); }
    if (events_) { delete[] events_; }
}


void ConnLoop::loop()
{
    running_ = true;
    int n_event = 0;

    while (running_) {
        n_event = epoll_wait(epfd_, events_, srv_conf_->epoll_max_events_, epoll_wait_timeout_);

        // 如果等待事件失败，且不是因为系统中断造成的，
        // 直接退出主循环
        if (n_event < 0 && errno != EINTR) {
            // SPDLOG_ERROR("epoll_wait error: {}", strerror(errno));
            running_ = false;
            break;
        }

        for (int i = 0; i < n_event; ++i) {
            int sockfd = events_[i].data.fd;
            
            if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                handle_conn_close(sockfd);
            } else {
                //TODO 放这儿对吗？
                auto conn = conns_.find(sockfd);
                if (conn == conns_.end()) {
                    //TODO 是否还要透传serverconf?
                    conns_.emplace(sockfd, std::make_shared<UserConn>(this, srv_conf_, sockfd));
                    conn = conns_.find(sockfd);
                }

                if (events_[i].events & EPOLLIN) {
                    handle_conn_in(sockfd, *(conn->second));
                } else if (events_[i].events & EPOLLOUT) {
                    handle_conn_out(sockfd, *(conn->second));
                }
            }
        }

        //BUG 多个读写任务是在同一个线程里顺序执行的，
        // 如果前一个读写任务阻塞了很长时间，很容易导致任务还没处理就超时了
        // 这个要想办法解决
        expired_.clear();
        timer_mgr_.handle_expired_timers(expired_);
        for (auto &sockfd : expired_) {
            handle_conn_close(sockfd);
        }
    }
}

void ConnLoop::stop()
{
    //TODO 需要改成发送事件，提高stop的效率
    running_ = false;
}

void ConnLoop::add_conn_event_read(int cli_sock)
{
    FdUtil::epoll_add_fd_oneshot(epfd_, cli_sock, EPOLLIN | EPOLLRDHUP);
    //TODO 允许自定义超时时间
    //TODO 加一个中间队列，避免定时器加锁，看看能否提高性能
    // 阻塞的write，是需要数据发到内核，还是需要对方收到才会返回
    timer_mgr_.add_timer(cli_sock);
}

void ConnLoop::mod_conn_event_read(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epfd_, cli_sock, EPOLLIN | EPOLLRDHUP);
    timer_mgr_.add_timer(cli_sock);
}

void ConnLoop::mod_conn_event_write(int cli_sock)
{
    FdUtil::epoll_mod_fd_oneshot(epfd_, cli_sock, EPOLLOUT | EPOLLRDHUP);
    timer_mgr_.add_timer(cli_sock);
}

void ConnLoop::conn_close(int cli_sock)
{
    handle_conn_close(cli_sock);
}

void ConnLoop::handle_conn_in(int cli_sock, UserConn &user_conn)
{
    timer_mgr_.add_timer(cli_sock);
    user_conn.process_in();
}

void ConnLoop::handle_conn_out(int cli_sock, UserConn &user_conn)
{
    timer_mgr_.add_timer(cli_sock);
    user_conn.process_out();
}

void ConnLoop::handle_conn_close(int cli_sock)
{
    FdUtil::epoll_del_fd(epfd_, cli_sock);
    conns_.erase(cli_sock);
    timer_mgr_.rm_timer(cli_sock);
    close(cli_sock);
}
