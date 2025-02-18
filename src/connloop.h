#ifndef SRC_CONNLOOP_H_
#include <vector>
#include <memory>
#include <mutex>

#include "timeutil.h"

#include "userconn.h"
#include "serverconf.h"

constexpr const int DEF_EPOLL_WAIT_TIMEOUT = 10 * 1000;
constexpr const int DEF_CMD_BUFF_LEN = 1024; 


//TODO 优雅的回收所有socketfd？
class ConnLoop {
public:
    /**
     * @brief 命令类型
     *        发送的时候采用的是char发送的，
     *        所以最大支持255个命令!!!!
     */
    enum class ConnLoopCmd {
        CMD_UNKNOWN = 0,
        CMD_ADD_FD = 1,
        CMD_CLOSE = 2
    };

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
    void add_clisock_to_queue(int cli_sock);
    void mod_conn_event_read(int cli_sock);
    void mod_conn_event_write(int cli_sock);
    void conn_close(int cli_sock);
    void cmd_send(ConnLoopCmd cmd);

private:
    void handle_conn_in(int cli_sock, UserConn &user_conn);
    void handle_conn_out(int cli_sock, UserConn &user_conn);
    void handle_conn_close(int cli_sock);
    void cmd_recv();
    void add_clisock_to_epoll();

private:
    const ServerConf *srv_conf_;
    bool stop_;
    const int epoll_wait_timeout_;
    int epfd_;
    struct epoll_event *events_;    
    std::unordered_map<int, std::shared_ptr<UserConn> > conns_;
    std::vector<int> expired_;
    TimerManager timer_mgr_;
    std::vector<int> new_cli_socks_;
    // 因为new_cli_socks_可能被多个线程同时访问，
    // 所以提供个用于交换数据的内部变量，提高访问性能
    std::vector<int> new_cli_socks_swap_;
    std::mutex new_cli_socks_mtx_;
    //TODO use pipe？
    int cmd_sockpair_[2];
    char cmd_r_buf_[DEF_CMD_BUFF_LEN];
};

#define SRC_CONNLOOP_H_
#endif  // SRC_CONNLOOP_H_
