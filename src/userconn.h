#ifndef SRC_USER_CONN_H_
#define SRC_USER_CONN_H_

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <atomic>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "serverconf.h"
#include "httpdata.h"

constexpr const int BUFFER_MIN_SIZE_R = 2048;

class ConnLoop;


class UserConn {
public:
    using HandleFunc = HttpResponse(*)(const HttpRequest&);
    static void register_err_handler(const HttpCode &code, HandleFunc func);
    static void register_router(const std::string &path, HttpMethod method, HandleFunc func);

public:
    enum ConnState {
        CONN_CONNECTED,
        CONN_DEALING,
    };

public:
    UserConn(ConnLoop *const connloop,
             const ServerConf *const conf,
             int cli_sock)
        : state_(ConnState::CONN_CONNECTED)
        , connloop_(connloop)
        , conf_(conf)
        , cli_sock_(cli_sock)
        , buffer_r_(BUFFER_MIN_SIZE_R, '\0')
        , buffer_r_bytes_(0)
        , req_parsed_bytes_(0)
        , rsp_(req_)
        , base_rsp_snd_(false)
        , body_snd_(false)
        , rsp_base_snd_bytes_(0)
        , rsp_body_snd_bytes_(0)
        , file_fd_(-1)
        , file_size_(0)
        {}
    ~UserConn();
    // 五法则，实现拷贝，移动，析构中的任意一个，都需要将其他四个实现
    // 但目前这个类暂时用不到拷贝和移动，所以直接实现成delete
    UserConn(const UserConn &) = delete;
    UserConn &operator=(const UserConn &) = delete;
    UserConn(UserConn &&) = delete;
    UserConn &operator=(UserConn &&) = delete;

public:
    void process_in();
    void process_out();
    void set_state(ConnState state) { state_ = state; }
    ConnState get_state() {return state_; }

private:
    bool recv_from_cli();
    void route_path();
    void close_file_fd() {
        if (file_fd_!= -1) {
            close(file_fd_);
            file_fd_ = -1;
            file_size_ = 0;
        }
    }
    bool send_to_cli();
    void send_base_rsp();
    void send_body();
    /**
     * @brief 重置连接状态
     *        当完成一次完整的收发请求后，如果该链接需要继续使用，就需要重置连接状态
     */
    void conn_state_reset() {
        buffer_r_bytes_ = 0;
        req_.reset();
        req_parsed_bytes_ = 0;
        rsp_.reset();
        state_ = ConnState::CONN_CONNECTED;
        base_rsp_snd_ = false;
        body_snd_ = false;
        rsp_base_snd_bytes_ = 0;
        rsp_body_snd_bytes_ = 0;
        close_file_fd();
    }

private:
    static std::map<HttpCode, HandleFunc> err_handler_;
    static std::map<std::string, std::map<HttpMethod, HandleFunc> > router_;

private:
    std::atomic<ConnState> state_;
    ConnLoop *const connloop_;
    const ServerConf *const conf_;
    int cli_sock_;
    std::string buffer_r_;
    std::size_t buffer_r_bytes_;
    HttpRequest req_;
    uint32_t req_parsed_bytes_;
    HttpResponse rsp_;
    bool base_rsp_snd_;
    bool body_snd_;
    uint32_t rsp_base_snd_bytes_;
    off_t rsp_body_snd_bytes_;
    int file_fd_;
    off_t file_size_;
};

#endif  // SRC_USER_CONN_H_
