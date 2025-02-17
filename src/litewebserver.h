#ifndef SRC_LITEWEBSERVER_H_
#define SRC_LITEWEBSERVER_H_

#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>

#include <sys/epoll.h>

#include "connloop.h"
#include "serverconf.h"
#include "ChaosThreadPool.h"
#include "timer.h"
#include "userconn.h"
#include "cppver.h"


class LiteWebServer
{
private:
    static int exit_event_;
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

public:
    void start_loop();

private:
    void init_log();
    void create_listen_service();
    /**
     * @brief 处理退出信号
     */
    void register_exit_signal();
    void ignore_SIGPIPE();
    /**
     * @brief 分配新连接给线程池中的一个线程处理
     */
    LWS_DEPRECATED
    void deal_new_conn();
    /**
     * @brief 分配新连接给线程池中的一个线程处理
     *        贪心的，一次accept完多个链接
     */
    void deal_new_conn_greedy();

private:
    const ServerConf srv_conf_;
    int epoll_fd_;
    int srv_sock_;
    chaos::ThreadPool eventpool_;
    struct epoll_event *events_;
    std::vector<std::shared_ptr<ConnLoop> > conn_loops_;
    uint8_t pool_idx_;
};

#endif //SRC_LITEWEBSERVER_H_
