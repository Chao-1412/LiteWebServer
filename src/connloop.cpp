#include "connloop.h"

#include <stdexcept>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "fdutil.h"


ConnLoop::ConnLoop(const ServerConf *const srv_conf, int epoll_wait_timeout)
    : srv_conf_(srv_conf)
    , stop_(false)
    , epoll_wait_timeout_(epoll_wait_timeout)
    , epfd_(epoll_create1(EPOLL_CLOEXEC))
    , events_(new struct epoll_event[srv_conf_->epoll_max_events_])
    , conns_(10000)
    , cmd_sockpair_{-1, -1}
    , cmd_r_buf_{0}
{
    expired_.reserve(10000);
    new_cli_socks_.reserve(10000);
    new_cli_socks_swap_.reserve(10000);

    if (epfd_ == -1) { throw std::runtime_error(strerror(errno)); }
    
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, cmd_sockpair_) == -1) {
        throw std::runtime_error(strerror(errno));
    }
    // 命令接收的socket设置为非阻塞，添加到epoll中
    // 命令发送的默认阻塞，这样当命令发满时会阻塞，保证不会丢失命令
    // 其实也可以是非阻塞，可以提高命令发送的效率，
    // 因为现在就只有两个命令，实际上命令丢几条影响不大
    // 不过如果丢失的是停止命令，可能导致线程停止较慢
    //TODO 后续测试下使用非阻塞命令效率是否有提升
    FdUtil::set_nonblocking(cmd_sockpair_[1]);
    FdUtil::epoll_add_fd(epfd_, cmd_sockpair_[1], EPOLLIN | EPOLLRDHUP | EPOLLET);
}

ConnLoop::~ConnLoop()
{
    if (epfd_ >= 0) { close(epfd_); }
    if (events_) { delete[] events_; }
    if (cmd_sockpair_[0] >= 0) { close(cmd_sockpair_[0]); }
    if (cmd_sockpair_[1] >= 0) { close(cmd_sockpair_[1]); }
}


void ConnLoop::loop()
{
    int n_event = 0;

    while (true) {
        n_event = epoll_wait(epfd_, events_, srv_conf_->epoll_max_events_, epoll_wait_timeout_);

        // 如果等待事件失败，且不是因为系统中断造成的，
        // 直接退出主循环
        if (n_event < 0 && errno != EINTR) {
            // SPDLOG_ERROR("epoll_wait error: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < n_event; ++i) {
            int sockfd = events_[i].data.fd;
            
            if (sockfd == cmd_sockpair_[1]) {
                cmd_recv();
                if (stop_) { return; }
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
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
    cmd_send(ConnLoopCmd::CMD_CLOSE);
}

void ConnLoop::add_clisock_to_queue(int cli_sock)
{
    {
        std::lock_guard<std::mutex> lock(new_cli_socks_mtx_);
        new_cli_socks_.push_back(cli_sock);
    }

    cmd_send(ConnLoopCmd::CMD_ADD_FD);
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

void ConnLoop::cmd_send(ConnLoopCmd cmd)
{
    //BUG 这里因为是阻塞的，所以才能直接取形参的地址，
    //    如果是非阻塞的将会引发BUG需要注意！
    char snd = static_cast<char>(cmd);
    int ret = write(cmd_sockpair_[0], &snd, sizeof(snd));
    if (ret != sizeof(snd)) {
        // SPDLOG_ERROR("write cmd_sockpair_[0] error: {}", strerror(errno));
    }
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

void ConnLoop::cmd_recv()
{
    while (true) {
        bool cmd_recv_add_fd = false;
        int ret = read(cmd_sockpair_[1], cmd_r_buf_, sizeof(cmd_r_buf_));
        // ret == 0 EOF
        // ret == -1:
        //      errno == EAGAIN 没有数据可读
        //      errno == EINTR 系统中断
        //      errno == EWOULDBLOCK 没有数据可读
        // 除了系统中断外，都应该直接退出，目前是这样的
        if (ret <= 0) {
            if (errno == EINTR) { continue; }
            else { break; }
        }

        // 处理每一个命令
        for (int i = 0; i < ret; ++i) {
            switch (cmd_r_buf_[i]) {
                case static_cast<char>(ConnLoopCmd::CMD_CLOSE): {
                    stop_ = true;
                    return;
                }
                case static_cast<char>(ConnLoopCmd::CMD_ADD_FD): {
                    if (!cmd_recv_add_fd) {
                        add_clisock_to_epoll();
                        cmd_recv_add_fd = true;
                    }
                    break;
                }
                default: break;
            }
        }
    }
}

void ConnLoop::add_clisock_to_epoll()
{
    new_cli_socks_swap_.clear();
    {
        std::lock_guard<std::mutex> lock(new_cli_socks_mtx_);
        new_cli_socks_swap_.swap(new_cli_socks_);
    }

    for (auto &clisock : new_cli_socks_swap_) {
        FdUtil::epoll_add_fd_oneshot(epfd_, clisock, EPOLLIN | EPOLLRDHUP);
        timer_mgr_.add_timer(clisock);
    }
}
