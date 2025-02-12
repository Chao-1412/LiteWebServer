#ifndef SRC_LITEWEBSERVER_H_
#define SRC_LITEWEBSERVER_H_

#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>

#include <sys/epoll.h>

#include "serverconf.h"
#include "ChaosThreadPool.h"
#include "timer.h"
#include "userconn.h"

constexpr const int DEF_TIMEOUT_S = 10 * 1000;


class LiteWebServer
{
    friend class UserConn;
private:
    static int exit_event;
    static void handle_signal(int sig);

public:
    /**
     * @brief 先实现成不可拷贝，不可移动的
     */
    //TODO
    LiteWebServer(const ServerConf srv_conf);
    LiteWebServer(const LiteWebServer&) = delete;
    LiteWebServer(LiteWebServer&&) = delete;
    LiteWebServer& operator=(const LiteWebServer&) = delete;
    LiteWebServer& operator=(LiteWebServer&&) = delete;
    ~LiteWebServer();

    bool is_running();
    void start_loop();

private:
    void init_log();
    void create_listen_serve();
    /**
     * @brief 处理退出信号
     */
    void register_exit_signal();
    void ignore_SIGPIPE();
    void deal_new_conn();
    void disconn_one(int cli_sock);
    void deal_conn_in(int cli_sock);
    void deal_conn_out(int cli_sock);
    void deal_conn_expired(int cli_sock);
    void modify_conn_event_read(int cli_sock);
    void modify_conn_event_write(int cli_sock);
    void modify_conn_event_close(int cli_sock);

private:
    const ServerConf srv_conf_;
    TimerManager timer_mgr_;
    std::vector<int> expired_;
    bool running_;
    int epoll_fd_;
    int srv_sock_;
    chaos::ThreadPool workerpool_;
    struct epoll_event *events_;
    std::unordered_map<int, std::shared_ptr<UserConn> > conns_;
};

#endif //SRC_LITEWEBSERVER_H_
