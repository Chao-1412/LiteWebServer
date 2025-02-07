#include "userconn.h"

#include <string>

#include <sys/socket.h>
#include <sys/sendfile.h>

#include "spdlog/spdlog.h"

#include "litewebserver.h"
#include "timer.h"
#include "debughelper.h"


std::map<HttpCode, UserConn::HandleFunc> UserConn::err_handler_ = {
    {HttpCode::BAD_REQUEST, err_handler_400},
    {HttpCode::NOT_FOUND, err_handler_404},
    {HttpCode::NOT_ALLOWED, err_handler_405},
    {HttpCode::INTERNAL_SERVER_ERROR, err_handler_500}
};
std::map<std::string, std::map<HttpMethod, UserConn::HandleFunc> > UserConn::router_;

void UserConn::register_err_handler(const HttpCode &code, HandleFunc func)
{
    err_handler_[code] = func;
}

void UserConn::register_router(const std::string &path, HttpMethod method, HandleFunc func)
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
        // SPDLOG_DEBUG("recv failed, regist read event, cli_sock: {}", cli_sock_);
        srv_inst_->modify_conn_event_read(cli_sock_);
        goto EXIT;
    }
    // SPDLOG_DEBUG("recv from client, cli_sock: {}, data: {}", cli_sock_, buffer_data_to_str(buffer_r_, buffer_r_bytes_));

    // 解析收到的数据
    req_parsed_bytes_ += req_.parse(buffer_r_, req_parsed_bytes_);
    if (!req_.parse_complete()) {
        // SPDLOG_DEBUG("parse failed, regist read event, cli_sock: {}", cli_sock_);
        srv_inst_->modify_conn_event_read(cli_sock_);
        goto EXIT;
    }

    // 返回数据生成后注册epoll写事件，待可写事件触发后
    // 会调用UserConn::process_out，进行处理
    srv_inst_->modify_conn_event_write(cli_sock_);
    // SPDLOG_DEBUG("parse successed, regist write event, cli_sock: {}", cli_sock_);

EXIT:
    set_state(ConnState::CONN_CONNECTED);
}

void UserConn::process_out()
{
    route_path();

    // 如果是文件类型，先打开文件
    // 打不开的话，返回500错误
    if (rsp_.body_is_file()) {
        std::string file_path = conf_->www_root_path_ + rsp_.get_body();
        file_fd_ = open(file_path.c_str(), O_RDONLY);
        if (file_fd_ < 0) {
            rsp_ = err_handler_[HttpCode::INTERNAL_SERVER_ERROR](req_);
            // SPDLOG_INFO("open file failed, code: {}, msg: {}", errno, strerror(errno));
        }
        struct stat file_stat;
        fstat(file_fd_, &file_stat);
        file_size_ = file_stat.st_size;
        rsp_.header_oper(HttpResponse::HeaderOper::MODIFY,
                         "Content-Length", std::to_string(file_size_));
    }

    if (!send_to_cli()) {
        srv_inst_->modify_conn_event_write(cli_sock_);
    } else {
        std::string conn_state;
        req_.get_header("Connection", conn_state);
        if (conn_state == "close") {
            // SPDLOG_DEBUG("Client {} want to close connection", cli_sock_);
            // 这里将计时器更新为当前时间让其立马过期，
            // 并让主线程在下次处理过期的定时器时关闭该链接
            // 不过目前定时器是根据epoll_wait的返回来调度的
            // 最后的一个链接可能不会马上被关闭，而是等到epoll_wait超时才会被关闭
            srv_inst_->timer_mgr_.update_timer(cli_sock_, SteadyClock::now());
            srv_inst_->modify_conn_event_close(cli_sock_);
        } else {
            // 如果不是直接断开链接，则重置连接状态
            // SPDLOG_DEBUG("Client {} want to keep alive", cli_sock_);
            conn_state_reset();
            srv_inst_->modify_conn_event_read(cli_sock_);
        }
    }

// EXIT:
    set_state(ConnState::CONN_CONNECTED);
}

//BUG 需要考虑数据很大，撑爆缓冲区满的情况
//TODO 最好一次性把数据读完，而不是每次epollin读一次，优化性能
bool UserConn::recv_from_cli()
{
    size_t remain_size = buffer_r_.size() - buffer_r_bytes_;
    char *read_begin = &(*buffer_r_.begin()) + buffer_r_bytes_;

    ssize_t recv_bytes = recv(cli_sock_, read_begin, remain_size, 0);

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
        buffer_r_bytes_ += recv_bytes;
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

    while (true) {
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
            // SPDLOG_ERROR("send to client failed, code: {}, msg: {}", errno, strerror(errno));
            return;
        }
        rsp_base_snd_bytes_ += send_bytes;
    }
}

void UserConn::send_body()
{
    ssize_t send_bytes = 0;

    while (true) {
        if (rsp_.body_is_file()) {
            send_bytes = file_size_ - rsp_body_snd_bytes_;
            if (send_bytes <= 0) {
                body_snd_ = true;
                close_file_fd();
                return;
            }
            if (send_bytes > HTTP_FILE_CHUNK_SIZE) {
                send_bytes = HTTP_FILE_CHUNK_SIZE;
            }
            send_bytes = sendfile(cli_sock_, file_fd_, &rsp_body_snd_bytes_, send_bytes);
            if (send_bytes <= 0) {
                // 主线程会根据epoll触发的事件进行处理
                //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
                // SPDLOG_ERROR("send to client failed, code: {}, msg: {}", errno, strerror(errno));
                return;
            }
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
                // SPDLOG_ERROR("send to client failed, code: {}, msg: {}", errno, strerror(errno));
                return;
            }
            rsp_body_snd_bytes_ += send_bytes;
        }
    }
}
