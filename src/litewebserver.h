#ifndef SRC_LITEWEBSERVER_H_
#define SRC_LITEWEBSERVER_H_

#include <string>
#include <unordered_map>
#include <memory>

#include <sys/epoll.h>

#include "serverconf.h"
#include "ChaosThreadPool.h"
#include "userconn.h"


class LiteWebServer
{
    friend class UserConn;
private:
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
    void register_signal();
    void deal_new_conn();
    void disconn_one(int cli_sock);
    void deal_conn_in(int cli_sock);
    void deal_conn_out(int cli_sock);
    void modify_conn_event_read(int cli_sock);
    void modify_conn_event_write(int cli_sock);

private:
    const ServerConf srv_conf_;
    bool running_;
    int epoll_fd_;
    int srv_sock_;
    chaos::ThreadPool workerpool_;
    struct epoll_event *events_;
    std::unordered_map<int, std::unique_ptr<UserConn> > conn_;
};

#endif //SRC_LITEWEBSERVER_H_
