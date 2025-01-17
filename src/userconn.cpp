#include "userconn.h"

#include <sys/socket.h>

#include <spdlog/spdlog.h>

#include "litewebserver.h"


void UserConn::process_in()
{
    bool ret = recv_from_cli();
    if (ret != true) {
        srv_inst_->modify_conn_event_read(cli_sock_);
        return;
    }
    // 解析收到的数据
    // 生成返回数据

    // 返回数据生成后注册epoll写事件，待可写事件触发后
    // 会调用UserConn::process_out，进行处理
    srv_inst_->modify_conn_event_write(cli_sock_);
}

void UserConn::process_out()
{
    srv_inst_->modify_conn_event_read(cli_sock_);
}

//BUG 需要考虑数据很大，撑爆缓冲区满的情况
bool UserConn::recv_from_cli()
{
    size_t remain_size = buffer_r_.size() - recv_data_size_;
    char *read_begin = buffer_r_.data() + recv_data_size_;

    ssize_t recv_bytes = recv(cli_sock_, read_begin, remain_size, 0);

    if (recv_bytes <= 0) {
        // 不管是 == 0 ，客户端关闭连接
        // 还是 < 0,
        // EAGAIN EWOULDBLOCK 重试
        // EINTR 系统中断
        // 都返回false，在main线程epoll池中会对连接进行处理
        return false;
    } else {
        recv_data_size_ += recv_bytes;
    }

    return true;
}

void UserConn::parse_data()
{
}
