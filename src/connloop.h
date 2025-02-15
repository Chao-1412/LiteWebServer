#ifndef SRC_CONNLOOP_H_
#include <atomic>
#include <vector>
#include <memory>

#include <timer.h>

#include "userconn.h"
#include "serverconf.h"

constexpr const int DEF_MAX_EVENTS = 4096;
constexpr const int DEF_EPOLL_WAIT_TIMEOUT = 10 * 1000;


class ConnLoop {
    friend class UserConn;

public:
    ConnLoop(const ServerConf *const srv_conf,
             int epoll_wait_timeout = DEF_EPOLL_WAIT_TIMEOUT);
    /**
     * @brief 五法则，实现拷贝，移动，析构中的任意一个，都需要将其他四个实现
     *        但目前这个类暂时用不到拷贝和移动，所以直接实现成delete
     */
    ConnLoop(const ConnLoop&) = delete;
    ConnLoop(ConnLoop&&) = delete;
    ConnLoop& operator=(const ConnLoop&) = delete;
    ConnLoop& operator=(ConnLoop&&) = delete;
    ~ConnLoop();

public:
    void loop();
    void stop();
    void add_conn_event_read(int cli_sock);
    void mod_conn_event_read(int cli_sock);
    void mod_conn_event_write(int cli_sock);
    void conn_close(int cli_sock);

private:
    void handle_conn_in(int cli_sock, UserConn &user_conn);
    void handle_conn_out(int cli_sock, UserConn &user_conn);
    void handle_conn_close(int cli_sock);

private:
    const ServerConf *srv_conf_;
    std::atomic_bool running_;
    const int epoll_wait_timeout_;
    int epfd_;
    struct epoll_event *events_;    
    std::unordered_map<int, std::shared_ptr<UserConn> > conns_;
    std::vector<int> expired_;
    TimerManager timer_mgr_;
};

#define SRC_CONNLOOP_H_
#endif  // SRC_CONNLOOP_H_
