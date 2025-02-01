#ifndef SRC_USER_CONN_H_
#define SRC_USER_CONN_H_

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>

#include "serverconf.h"
#include "httpdata.h"

#define HTTP_FILE_CHUNK_SIZE 64 * 1024

class LiteWebServer;


class UserConn {
public:
    using HandleFunc = HttpResponse(*)(const HttpRequest&);
    static void register_router(const std::string &path, HttpMethod method,HandleFunc func);
    static void static_process_in(std::shared_ptr<UserConn> conn);
    static void static_process_out(std::shared_ptr<UserConn> conn);

public:
    UserConn(LiteWebServer *const srv_inst,
             const ServerConf *const conf,
             int cli_sock)
        : srv_inst_(srv_inst)
        , conf_(conf)
        , cli_sock_(cli_sock)
        , buffer_r_(conf_->buffer_size_r_, '\0')
        , buffer_r_bytes_(0)
        , req_parsed_bytes_(0)
        , rsp_(req_)
        , closed_(false)
        , base_rsp_snd_(false)
        , body_snd_(false)
        , rsp_base_snd_bytes_(0)
        , rsp_body_snd_bytes_(0)
        , file_fd_(-1)
        , file_size_(0)
        {}
    ~UserConn();
    // 五法则，实现拷贝，移动，析构中的任意一个，都需要将其他四个实现
    UserConn(const UserConn &) = delete;
    UserConn &operator=(const UserConn &) = delete;
    UserConn(UserConn &&) = delete;
    UserConn &operator=(UserConn &&) = delete;

public:
    void process_in();
    void process_out();
    void close_conn() { std::lock_guard<std::mutex> lock(mtx_); closed_ = true; }

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
        closed_ = false;
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
    LiteWebServer *const srv_inst_;
    const ServerConf *const conf_;
    int cli_sock_;
    std::string buffer_r_;
    std::size_t buffer_r_bytes_;
    HttpRequest req_;
    uint32_t req_parsed_bytes_;
    HttpResponse rsp_;
    bool closed_;
    std::mutex  mtx_;
    bool base_rsp_snd_;
    bool body_snd_;
    uint32_t rsp_base_snd_bytes_;
    off_t rsp_body_snd_bytes_;
    int file_fd_;
    off_t file_size_;
};

#endif  // SRC_USER_CONN_H_
