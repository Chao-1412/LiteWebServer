#ifndef SRC_SERVER_CONF_H_
#define SRC_SERVER_CONF_H_

#include <string>

#include <cstdint>

class ServerConf
{
public:
    ServerConf(uint16_t port,
               uint8_t nthread = 4,
               int listen_queue_n = 5,
               bool epoll_et_srv = false,
               bool epoll_et_conn = false,
               uint16_t epoll_max_events = 100,
               uint16_t buffer_size_r = 2048,
               std::string www_root_path = "/var/www")
        : port_(port)
        , nthread_(nthread)
        , listen_queue_n_(listen_queue_n)
        , epoll_et_srv_(epoll_et_srv)
        , epoll_et_conn_(epoll_et_conn)
        , epoll_max_events_(epoll_max_events)
        , buffer_size_r_(buffer_size_r)
        , www_root_path_(www_root_path)
        {/* TODO 校验一下参数是否可用 */};

public:
    uint16_t port_;
    uint8_t nthread_;
    int listen_queue_n_;
    //TODO et mode not implemented now
    bool epoll_et_srv_;
    bool epoll_et_conn_;
    uint16_t epoll_max_events_;
    uint16_t buffer_size_r_;
    std::string www_root_path_;
};

#endif //SRC_SERVER_CONF_H_
