#include "userconn.h"

#include <string>

#include <sys/socket.h>

#include <spdlog/spdlog.h>

#include "litewebserver.h"

std::map<HttpCode, UserConn::HandleFunc> UserConn::err_handler_ = {
    {HttpCode::BAD_REQUEST, err_handler_400},
    {HttpCode::NOT_FOUND, err_handler_404},
    {HttpCode::NOT_ALLOWED, err_handler_405},
    {HttpCode::INTERNAL_SERVER_ERROR, err_handler_500}
};
std::map<std::string, std::map<HttpMethod, UserConn::HandleFunc> > UserConn::router_;

void UserConn::register_router(const std::string &path, HttpMethod method,HandleFunc func)
{
    router_[path][method] = func;
}

void UserConn::static_process_in(std::shared_ptr<UserConn> conn)
{
    conn->process_in();
}

void UserConn::static_process_out(std::shared_ptr<UserConn> conn)
{
    conn->process_out();
}

UserConn::~UserConn()
{
    close_file_fd();
}

void UserConn::process_in()
{
    // 接收数据
    bool ret = recv_from_cli();
    if (ret != true) {
        srv_inst_->modify_conn_event_read(cli_sock_);
        return;
    }
    // 解析收到的数据
    req_parsed_bytes_ += req_.parse(buffer_r_, req_parsed_bytes_);
    if (!req_.parse_complete()) {
        srv_inst_->modify_conn_event_read(cli_sock_);
        return;
    }

    // 返回数据生成后注册epoll写事件，待可写事件触发后
    // 会调用UserConn::process_out，进行处理
    srv_inst_->modify_conn_event_write(cli_sock_);
}

void UserConn::process_out()
{
    route_path();

    if (rsp_.body_is_file()) {
        std::string file_path = conf_->www_root_path_ + rsp_.get_body();
        file_fd_ = open(file_path.c_str(), O_RDONLY);
        if (file_fd_ < 0) {
            rsp_ = err_handler_[HttpCode::INTERNAL_SERVER_ERROR](req_);
        }
    }

    if (!send_to_cli()) {
        srv_inst_->modify_conn_event_write(cli_sock_);
    } else {
        std::string conn_state;
        rsp_.get_header("Connection", conn_state);
        if (conn_state == "close") {
            srv_inst_->disconn_one(cli_sock_);
        } else {
            // 如果不是直接断开链接，则重置连接状态
            conn_state_reset();
            srv_inst_->modify_conn_event_read(cli_sock_);
        }
    }
}

//BUG 需要考虑数据很大，撑爆缓冲区满的情况
//TODO 最好一次性把数据读完，而不是每次epollin读一次，优化性能
bool UserConn::recv_from_cli()
{
    size_t remain_size = conf_->buffer_size_r_ - buffer_r_.size();
    char *read_begin = buffer_r_.data() + buffer_r_.size();

    ssize_t recv_bytes = 0;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (closed_) {
            return false;
        }
        recv_bytes = recv(cli_sock_, read_begin, remain_size, 0);
    }

    if (recv_bytes <= 0) {
        // 不管是 == 0：客户端关闭连接
        // 还是 < 0：
        // EAGAIN EWOULDBLOCK 重试
        // EINTR 系统中断
        // 先都返回false，
        // 主线程会根据epoll触发的事件进行处理
        //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
        return false;
    } else {
        buffer_r_.resize(buffer_r_.size() + recv_bytes);
    }

    return true;
}

void UserConn::route_path()
{
    if (req_.is_bad_req()) {
        rsp_ = err_handler_[HttpCode::BAD_REQUEST](req_);
        return;
    } else {
        std::string path = req_.get_path();
        HttpMethod method = req_.get_method();
        const auto &it_path = router_.find(path);
        if (it_path == router_.end()) {
            rsp_ = err_handler_[HttpCode::NOT_FOUND](req_);
            return;
        }
        const auto &call_fn = it_path->second.find(method);
        if (call_fn == it_path->second.end()) {
            rsp_ = err_handler_[HttpCode::NOT_ALLOWED](req_);
            return;
        }

        rsp_ = call_fn->second(req_);
    }
}

bool UserConn::send_to_cli()
{
    if (!base_rsp_snd_) {
        send_base_rsp();
    }

    if (base_rsp_snd_ && !body_snd_) {
        send_body();
    }

    if (base_rsp_snd_ && body_snd_) {
        return true;
    }

    return false;
}

void UserConn::send_base_rsp()
{
    ssize_t send_bytes = 0;

    std::lock_guard<std::mutex> lock(mtx_);
    while (!closed_) {
        size_t remain_size = rsp_.get_base_rsp().size() - rsp_base_snd_bytes_;
        const char *snd_beg = rsp_.get_base_rsp().data() + rsp_base_snd_bytes_;

        // 理论上不会出现remain_size < 0的情况
        if (remain_size <= 0) {
            base_rsp_snd_ = true;
            return;
        }
        send_bytes = send(cli_sock_, snd_beg, remain_size, 0);
        if (send_bytes <= 0) {
            // 主线程会根据epoll触发的事件进行处理
            //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
            SPDLOG_ERROR("send to client failed, code: {}, msg: {}", errno, strerror(errno));
            return;
        }
        rsp_base_snd_bytes_ += send_bytes;
    }
}

void UserConn::send_body()
{
    ssize_t send_bytes = 0;

    std::lock_guard<std::mutex> lock(mtx_);
    while (!closed_) {
        if (rsp_.body_is_file()) {

        } else {
            size_t remain_size = rsp_.get_body().size() - rsp_body_snd_bytes_;
            const char *snd_beg = rsp_.get_body().data() + rsp_body_snd_bytes_;

            // 理论上不会出现remain_size < 0的情况
            if (remain_size <= 0) {
                body_snd_ = true;
                return;
            }
            send_bytes = send(cli_sock_, snd_beg, remain_size, 0);

            if (send_bytes <= 0) {
                // 主线程会根据epoll触发的事件进行处理
                //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
                SPDLOG_ERROR("send to client failed, code: {}, msg: {}", errno, strerror(errno));
                return;
            }
            rsp_body_snd_bytes_ += send_bytes;
        }
    }
}
