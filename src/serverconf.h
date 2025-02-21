#ifndef SRC_SERVER_CONF_H_
#define SRC_SERVER_CONF_H_

#include <string>

#include <cstdint>

#include "filepathutil.h"


class ServerConf
{
public:
    ServerConf(uint16_t port,
               std::string doc_root,
               uint8_t nthread = 4,
               int backlog = 1024,
               bool epoll_et_srv = false,
               bool epoll_et_conn = false,
               uint16_t epoll_max_events = 4096)
        : port_(port)
        , doc_root_(doc_root)
        , nthread_(nthread)
        , backlog_(backlog)
        , epoll_et_srv_(epoll_et_srv)
        , epoll_et_conn_(epoll_et_conn)
        , epoll_max_events_(epoll_max_events)
        {/* TODO 校验一下参数是否可用 */};

public:
    uint16_t port_;
    // document-root
    std::string doc_root_;
    uint8_t nthread_;
    // 完成队列（全连接队列）大小
    // 其余可以微调的参数包括：
    // sysctl -w net.core.somaxconn=1024（完成队列，系统级）
    // sysctl -w net.ipv4.tcp_max_syn_backlog=1024（半连接队列，系统级）
    int backlog_;
    bool epoll_et_srv_;
    //TODO conn et mode not implemented now
    bool epoll_et_conn_;
    uint16_t epoll_max_events_;
};

#endif //SRC_SERVER_CONF_H_
