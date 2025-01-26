#ifndef SRC_USER_CONN_H_
#define SRC_USER_CONN_H_

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <stdlib.h>
#include <stdint.h>

#include "serverconf.h"
#include "httpdata.h"

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
        , buffer_r_(std::string(conf_->buffer_size_r_, '\0'))
        , req_parsed_bytes_(0)
        , rsp_(req_)
        , closed_(false)
        , base_rsp_snd_(false)
        , body_snd_(false)
        , rsp_base_snd_bytes_(0)
        , rsp_body_snd_bytes_(0)
        {}
    void process_in();
    void process_out();
    void close_conn();

private:
    bool recv_from_cli();
    void route_path();
    bool send_to_cli();
    void send_base_rsp();
    void send_body();
    /**
     * @brief 重置连接状态
     *        当完成一次完整的收发请求后，如果该链接需要继续使用，就需要重置连接状态
     */
    void conn_state_reset();

private:
    static std::map<HttpCode, HandleFunc> err_handler_;
    static std::map<std::string, std::map<HttpMethod, HandleFunc> > router_;

private:
    LiteWebServer *const srv_inst_;
    const ServerConf *const conf_;
    int cli_sock_;
    std::string buffer_r_;
    HttpRequest req_;
    uint32_t req_parsed_bytes_;
    HttpResponse rsp_;
    bool closed_;
    std::mutex  mtx_;
    bool base_rsp_snd_;
    bool body_snd_;
    uint32_t rsp_base_snd_bytes_;
    uint32_t rsp_body_snd_bytes_;
};

#endif  // SRC_USER_CONN_H_
