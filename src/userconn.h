#ifndef SRC_USER_CONN_H_
#define SRC_USER_CONN_H_

#include <string>

#include <stdlib.h>

#include "serverconf.h"

class LiteWebServer;

class UserConn {
public:
    UserConn(LiteWebServer *const srv_inst,
             const ServerConf *const conf,
             int cli_sock)
        : srv_inst_(srv_inst)
        , conf_(conf)
        , cli_sock_(cli_sock)
        , buffer_r_(std::string(conf_->buffer_size_r_, '\0'))
        , recv_data_size_(0)
        , buffer_w_(std::string(conf_->buffer_size_w_, '\0'))
        , send_data_size_(0)
        {}
    void process_in();
    void process_out();
    bool recv_from_cli();
    bool send_to_cli();
    void parse_data();

private:
    LiteWebServer *const srv_inst_;
    const ServerConf *const conf_;
    int cli_sock_;
    std::string buffer_r_;
    size_t recv_data_size_;
    std::string buffer_w_;
    size_t send_data_size_;
};

#endif  // SRC_USER_CONN_H_
