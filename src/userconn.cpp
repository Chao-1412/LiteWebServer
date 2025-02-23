#include "userconn.h"

#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "spdlog/spdlog.h"

#include "litewebserver.h"
#include "timeutil.h"
#include "filepathutil.h"
// #include "debughelper.h"

//BUG 目前chunk size不能大于系统缓冲区大小，否则永远无法写入数据
constexpr const int HTTP_FILE_CHUNK_SIZE = 64 * 1024;


std::map<HttpCode, UserConn::HandleFunc> UserConn::err_handler_ = {
    {HttpCode::MOVED_PERMANENTLY, err_handler_301},
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

UserConn::~UserConn()
{
    close_file_fd();
}

void UserConn::process_in()
{
    // 接收数据
    bool ret = recv_from_cli();
    if (ret != true) {
        connloop_->mod_conn_event_read(cli_sock_);
        return;
    }
    // SPDLOG_DEBUG("recv from client, cli_sock: {}, data: {}",
    //              cli_sock_, buffer_data_to_str(buffer_r_, buffer_r_bytes_));

    // 解析收到的数据
    req_parsed_bytes_ += req_.parse(buffer_r_, req_parsed_bytes_);
    if (!req_.parse_complete()) {
        connloop_->mod_conn_event_read(cli_sock_);
        return;
    }

    // SPDLOG_DEBUG("request current data: {}", req_.dump_data_str());

    // 返回数据生成后注册epoll写事件，待可写事件触发后
    // 会调用UserConn::process_out，进行处理
    connloop_->mod_conn_event_write(cli_sock_);
}

void UserConn::process_out()
{
    route_path();

    // SPDLOG_DEBUG("response current data: {}", rsp_.dump_data_str());

    // 如果是文件类型，打开文件
    // 打不开的话，返回500错误
    //!!! 这里必须判断file_fd_是否是已经打开的，
    // 因为发送过程可能因为文件过大，系统缓冲区满而无法一次发完
    // 这时会重新注册EPOLLOUT事件，导致process_out重入，
    // 如果再次打开文件，旧的文件描述符会发生泄漏，
    // 出现越来越多的未关闭描述符
    if (rsp_.body_is_file() && file_fd_ == -1) {
        std::string file_path = combine_two_path(conf_->doc_root_, rsp_.get_body());
        file_fd_ = open(file_path.c_str(), O_RDONLY);
        if (file_fd_ >= 0) {
            struct stat file_stat;
            fstat(file_fd_, &file_stat);
            // 如果是路径的话，返回301错误
            // 记得关闭文件描述符
            if (file_stat.st_mode & S_IFDIR) {
                close_file_fd();
                rsp_ = err_handler_[HttpCode::MOVED_PERMANENTLY](req_);
            } else {
                file_size_ = file_stat.st_size;
                rsp_.header_oper(HttpResponse::HeaderOper::MODIFY,
                                "Content-Length", std::to_string(file_size_));
            }
        } else {
            if (errno == ENOENT) {
                rsp_ = err_handler_[HttpCode::NOT_FOUND](req_);
            } else {
                rsp_ = err_handler_[HttpCode::INTERNAL_SERVER_ERROR](req_);
                SPDLOG_ERROR("{} open failed, code: {}, msg: {}", file_path, errno, strerror(errno));
            }
        }
    }

    if (!send_to_cli()) {
        connloop_->mod_conn_event_write(cli_sock_);
    } else {
        std::string conn_state;
        if (req_.get_header("Connection", conn_state))
        {
            StringUtil::str_to_lower(conn_state);
            if (conn_state == "keep-alive") {
                // 如果不是直接断开链接，则重置连接状态
                conn_state_reset();
                connloop_->mod_conn_event_read(cli_sock_);
            } else {
                connloop_->conn_close(cli_sock_);
            }
        } else {
            // 如果没有Connection字段，则默认断开链接
            connloop_->conn_close(cli_sock_);
        }
    }
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
        // 事件线程会根据epoll触发的事件进行处理
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
        if (it_path != router_.end()) {
            const auto &call_fn = it_path->second.find(method);
            if (call_fn != it_path->second.end()) {
                rsp_ = call_fn->second(req_);
            } else {
                rsp_ = err_handler_[HttpCode::NOT_ALLOWED](req_);
            }
        } else {
            // "/" "/foo/bar/" "/foo/"
            // 都认为是根目录
            if (req_.get_path().back() == '/') {
                rsp_ = root_handler(req_);
            } else {
                rsp_ = static_file_handler(req_);
            }
        }
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
            // 事件线程会根据epoll触发的事件进行处理
            //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
            return;
        }
        rsp_base_snd_bytes_ += send_bytes;
    }
}

void UserConn::send_body()
{
    ssize_t send_bytes = 0;

    if (rsp_.body_is_file()) {
        while (true) {
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
                // 事件线程会根据epoll触发的事件进行处理
                //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
                //TODO 调整系统缓冲区大小是否能提升性能？
                return;
            }
        }
    } else {
        while (true) {
            size_t remain_size = rsp_.get_body().size() - rsp_body_snd_bytes_;
            const char *snd_beg = rsp_.get_body().data() + rsp_body_snd_bytes_;

            // 理论上不会出现remain_size < 0的情况
            if (remain_size <= 0) {
                body_snd_ = true;
                return;
            }
            send_bytes = send(cli_sock_, snd_beg, remain_size, 0);

            if (send_bytes <= 0) {
                // 事件线程会根据epoll触发的事件进行处理
                //（目前考虑到的EAGAIN EWOULDBLOCK EINTR）都有处理，不知道还有没有其他的
                return;
            }
            rsp_body_snd_bytes_ += send_bytes;
        }
    }
}
