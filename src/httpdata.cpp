#include "httpdata.h"

#include <vector>
#include <iostream>
#include <utility>

#include <stdint.h>
#include <stdlib.h>

#include "stringutil.h"


HttpRequest::HttpRequest()
    : state_(ParseState::PARSE_REQ_LINE)
    , is_bad_req_(false)
    , method_(HttpMethod::UNKNOWN)
    , path_("")
    , http_ver_("HTTP/1.1")
    , headers_(10)   // 预分配大小，降低哈希开销，空间换时间
    , param_(10)
    , body_("")
    {}

uint32_t HttpRequest::parse(const std::string &data, uint32_t start_idx)
{
    // example:
    // GET /index.html HTTP/1.1\r\n
    // Host: www.example.com\r\n
    // Connection: close\r\n
    // \r\n

    // POST /submit-form HTTP/1.1\r\n
    // Host: www.example.com\r\n
    // Content-Type: application/x-www-form-urlencoded\r\n
    // Content-Length: 27\r\n
    // \r\n
    // field1=value1&field2=value2

    uint32_t parsed_bytes = 0;

    /**
     * 必须按照，LINE，HEADER，BODY的顺序解析
     */
    if (state_ == ParseState::PARSE_SUCCESS
        || start_idx >= data.size()) {
        return 0;
    }

    if (state_ == ParseState::PARSE_REQ_LINE) {
        parsed_bytes += parse_req_line(data, start_idx + parsed_bytes);
    }

    if (state_ == ParseState::PARSE_REQ_HEADER) {
        parsed_bytes += parse_req_header(data, start_idx + parsed_bytes);
    }

    if (state_ == ParseState::PARSE_BODY) {
        parsed_bytes += parse_req_body(data, start_idx + parsed_bytes);
    }

    return parsed_bytes;
}

bool HttpRequest::get_header(const std::string &key, std::string &val) const
{
    auto it = headers_.find(key);
    if (it == headers_.end()) {
        return false;
    }
    val = it->second;
    return true;
}

bool HttpRequest::get_param(const std::string &key, std::string &val) const
{
    auto it = param_.find(key);
    if (it == param_.end()) {
        return false;
    }

    val = it->second;
    return true;
}

void HttpRequest::dump_data()
{
    std::cout << "====================================" << std::endl;
    std::cout << "Current http req data:" << std::endl;
    std::cout << "is complete: " << parse_complete() << std::endl;
    std::cout << "is bad req: " << is_bad_req() << std::endl;
    std::cout << "parse state: " << static_cast<int>(state_) << std::endl;
    std::cout << "method: " << static_cast<int>(method_)
              << " "
              << http_enum_to_str<HttpMethod>(method_)
              << std::endl;
    std::cout << "path: " << path_ << std::endl;
    std::cout << "http_ver: " << http_ver_ << std::endl;
    std::cout << "body: " << body_ << std::endl;
    
    std::cout << "header:" << std::endl;
    for (const auto &kv : headers_) {
        std::cout << "    " << kv.first << ": " << kv.second << std::endl;
    }

    std::cout << "param:" << std::endl;
    for (const auto &kv : param_) {
        std::cout << "    " << kv.first << ": " << kv.second << std::endl;
    }
}

std::string HttpRequest::dump_data_str()
{
    std::string data;
    data += "====================================\n";
    data += "Current http req data:\n";
    data += "is complete: " + std::to_string(parse_complete()) + "\n";
    data += "is bad req: " + std::to_string(is_bad_req()) + "\n";
    data += "parse state: " + std::to_string((int)state_) + "\n";
    data += "method: " + std::to_string((int)method_)
            + " "
            + http_enum_to_str<HttpMethod>(method_) + "\n";
    data += "path: " + path_ + "\n";
    data += "http_ver: " + http_ver_ + "\n";
    data += "body: " + body_ + "\n";

    data += "header:\n";
    for (const auto &kv : headers_) {
        data += "    " + kv.first + ": " + kv.second + "\n";
    }

    data += "param:\n";
    for (const auto &kv : param_) {
        data += "    " + kv.first + ": " + kv.second + "\n";
    }

    return data;
}

uint32_t HttpRequest::parse_req_line(const std::string &data, uint32_t start_idx)
{
    // 获取 请求行
    std::string line = "";
    std::size_t line_end = std::string::npos;

    //BUG 需要防止输入"\r \n"的情况
    line_end = data.find("\r\n", start_idx);
    if (line_end == std::string::npos) {
        return 0;
    }
    line = data.substr(start_idx, line_end - start_idx);
    

    // example:
    // GET /index.html HTTP/1.1\r\n
    std::string segment;
    std::size_t start_pos = 0;
    std::size_t pos = 0;

    // 处理 请求方法
    pos = line.find(" ");
    if (pos == std::string::npos) {
        set_bad_req();
        return 0;
    }

    segment = line.substr(start_pos, pos);
    method_ = http_str_to_enum<HttpMethod>(segment.c_str());
    if (method_ == HttpMethod::UNKNOWN) {
        set_bad_req();
        return 0;
    }

    // 处理 请求路径
    start_pos = pos + 1;
    pos = line.find(" ", start_pos);
    if (pos == std::string::npos) {
        set_bad_req();
        return 0;
    }

    segment = line.substr(start_pos, pos - start_pos);
    if (!parse_uri(segment)) {
        set_bad_req();
        return 0;
    }

    // 处理 HTTP版本
    start_pos = pos + 1;
    segment = line.substr(start_pos);
    HttpVersion http_ver = http_str_to_enum<HttpVersion>(segment.c_str());
    if (http_ver == HttpVersion::UNKNOWN) {
        set_bad_req();
        return 0;
    } else {
        http_ver_ = segment;
    }

    // 解析成功，状态机状态转移
    state_ = ParseState::PARSE_REQ_HEADER;
    // line size plus "\r\n"
    return line.size() + 2;
}

bool HttpRequest::parse_uri(const std::string &uri)
{
    bool get_token = false;
    std::string delim = "?";

    get_token = StringUtil::str_get_first_token(path_, uri, delim, 0);
    if (!get_token) {
        if (uri.empty()) {
            return false;
        } else {
            path_ = uri;
            return true;
        }
    }
    if (!path_is_vaild(path_)) {
        return false;
    }

    std::string raw_param = uri.substr(path_.size() + delim.size());
    if (raw_param.empty()) {
        // 明明带有参数的"?"，但是没有参数
        return false;
    }

    delim = "&";
    std::string key_val;
    std::size_t start_pos = 0;
    while (1) {
        get_token = StringUtil::str_get_first_token(key_val, raw_param, delim, start_pos);
        if (!get_token) {
            // 可能只有一个参数，或最后一个参数
            key_val = raw_param.substr(start_pos);
        }
        
        get_token = StringUtil::str_get_key_val(param_, key_val, "=");
        if (!get_token) {
            return false;
        }

        start_pos += key_val.size() + delim.size();
        if (start_pos >= raw_param.size()) {
            break;
        }
    }

    return true;
}

bool HttpRequest::path_is_vaild(const std::string &path)
{
    //TODO 可能需要更详细的检查路径
    return !path.empty();
}

uint32_t HttpRequest::parse_req_header(const std::string &data, uint32_t start_idx)
{
    uint32_t parsed_bytes = 0;
    std::string line;
    bool get_token = false;

    while(1) {
        get_token = StringUtil::str_get_first_token(
                        line, data, "\r\n",
                        start_idx + parsed_bytes
                    );
        if (!get_token) {
            // 可能没有"\r\n"，说明数据量不足，需要等待下次调用
            return parsed_bytes;
        }

        if (line.empty()) {
            // 是空行, 说明请求头解析完毕
            parsed_bytes += 2;
            break;
        }
        //BUG 需要检查key-val的合法性
        get_token = StringUtil::str_get_key_val(headers_, line, ": ");
        if(!get_token) {
            set_bad_req();
            return parsed_bytes;
        }

        parsed_bytes += line.size() + 2;
    }

    //BUG 需要校验Host头是否存在
    // 解析成功，状态机状态转移
    state_ = ParseState::PARSE_BODY;
    // plus "\r\n" size
    return parsed_bytes;
}

uint32_t HttpRequest::parse_req_body(const std::string &data, uint32_t start_idx)
{
    uint32_t parsed_bytes = 0;

    if (method_ == HttpMethod::GET) {
        // GET请求没有请求体
        state_ = ParseState::PARSE_SUCCESS;
        return 0;
    } else if (method_ == HttpMethod::POST) {
        //TODO 封装成独立函数
        //BUG 目前只支持有Content-Length的POST请求
        //     不含Content-Length的默认包体大小为0
        const auto &header = headers_.find("Content-Length");
        if (header == headers_.end()) {
            state_ = ParseState::PARSE_SUCCESS;
            return 0;
        }
        // 有Content-Length，但是数据量不足
        if (start_idx >= data.size()) {
            return 0;
        }
        unsigned long content_len = 0;
        //TODO 优化每一次都要转换的问题
        bool converted = StringUtil::str_to_inum(content_len, header->second, strtoul);
        if (!converted) {
            set_bad_req();
            return 0;
        }
        // 不会出现小等于0的情况
        unsigned long remain_body_len = content_len - body_.size();
        if (remain_body_len > data.size() - start_idx) {
            remain_body_len = data.size() - start_idx;
        }
        body_.append(data.substr(start_idx, remain_body_len));
        parsed_bytes += remain_body_len;
        if (body_.size() == content_len) {
            state_ = ParseState::PARSE_SUCCESS;
        }
    } else {
        //BUG 支持其他类型的请求方法
        set_bad_req();
        return 0;
    }

    return parsed_bytes;
}


HttpResponse::HttpResponse(const HttpRequest &req)
    : http_ver_(req.get_http_ver())
    , code_(HttpCode::OK)
    , headers_({{"Content-Type", "text/html; charset=UTF-8"},
                {"Connection", "close"}}, 10)   // 预分配大小，降低哈希开销，空间换时间
    , maked_base_rsp_(false)
    , base_rsp_("")
    , body_("")
    , body_type_(HttpContentType::HTML_TYPE)
    , body_is_file_(true)
{
    req.get_header("Connection", headers_["Connection"]);
}

void HttpResponse::header_oper(HeaderOper oper, const std::string &key, const std::string &val)
{
    if (oper == HeaderOper::ADD || oper == HeaderOper::MODIFY) {
        headers_[key] = val;
        maked_base_rsp_ = false;
    } else if (oper == HeaderOper::DEL) {
        const auto &it = headers_.find(key);
        if (it != headers_.end()) {
            headers_.erase(it);
            maked_base_rsp_ = false;
        }
    } else if (oper == HeaderOper::CLEAR) {
        headers_.clear();
        maked_base_rsp_ = false;
    }
}

void HttpResponse::header_oper(HeaderOper oper, const std::string &key, std::string &&val)
{
    if (oper == HeaderOper::ADD || oper == HeaderOper::MODIFY) {
        headers_[key] = std::move(val);
        maked_base_rsp_ = false;
    } else if (oper == HeaderOper::DEL) {
        const auto &it = headers_.find(key);
        if (it != headers_.end()) {
            headers_.erase(it);
            maked_base_rsp_ = false;
        }
    } else if (oper == HeaderOper::CLEAR) {
        headers_.clear();
        maked_base_rsp_ = false;
    }
}

bool HttpResponse::get_header(const std::string &key, std::string &val) const
{
    const auto &it = headers_.find(key);
    if (it == headers_.end()) {
        return false;
    }
    val = it->second;
    return true;
}

void HttpResponse::set_body_file(const std::string &path, HttpContentType type, const std::string &charset)
{
    set_body(path, true);
    update_content_type(type, charset);
}

void HttpResponse::set_body_bin(const std::string &data, HttpContentType type, const std::string &charset)
{
    set_body(data, false);
    update_content_type(type, charset);
}

void HttpResponse::dump_data()
{
    std::cout << "====================================" << std::endl;
    std::cout << "Current http rsp data:" << std::endl;
    std::cout << "code: " << static_cast<int>(code_)
              << " "
              << http_enum_to_str<HttpCode>(code_)
              << std::endl;
    std::cout << "header:" << std::endl;
    for (const auto &kv : headers_) {
        std::cout << "    " << kv.first << ": " << kv.second << std::endl;
    }
}

std::string HttpResponse::dump_data_str()
{
    std::string data;
    data += "====================================\n";
    data += "Current http rsp data:\n";
    data += "code: " + std::to_string((int)code_)
            + " "
            + http_enum_to_str<HttpCode>(code_) + "\n";
    data += "header:\n";
    for (const auto &kv : headers_) {
        data += "    " + kv.first + ": " + kv.second + "\n";
    }

    return data;
}

void HttpResponse::make_base_rsp()
{
    if (maked_base_rsp_) {
        return;
    }

    base_rsp_.clear();
    base_rsp_ = http_ver_ + " "
                + std::to_string((int)code_) + " " + http_enum_to_str(code_) + "\r\n";
    for (const auto &kv : headers_) {
        base_rsp_ += kv.first + ": " + kv.second + "\r\n";
    }
    base_rsp_ += "\r\n";

    maked_base_rsp_ = true;
}

std::string HttpResponse::def_charset(HttpContentType type)
{
    switch (type) {
        case HttpContentType::HTML_TYPE:
        case HttpContentType::JSON_TYPE:
        case HttpContentType::CSS_TYPE:
            return "; charset=UTF-8";
        default:
            return "";
    }
}

void HttpResponse::update_content_type(HttpContentType type, const std::string &charset)
{
    body_type_ = type;

    if (!charset.empty()) {
        header_oper(HeaderOper::MODIFY, "Content-Type",
                    http_enum_to_str<HttpContentType>(type)
                      + std::string("; charset=") + charset);
    } else {
        header_oper(HeaderOper::MODIFY, "Content-Type",
                    http_enum_to_str<HttpContentType>(type)
                      + def_charset(type));
    }
}

void HttpResponse::set_body(const std::string &data, bool is_file)
{
    body_is_file_ = is_file;

    if (!body_is_file()) {
        // 不是文件类型，直接计算"Content-Length"，
        //!!! 文件类型需要在打开文件的时候手动的设置长度
        header_oper(HeaderOper::ADD, "Content-Length", std::to_string(data.size()));
    }
    body_ = data;
}

HttpResponse static_file_handler(const HttpRequest &req)
{
    HttpResponse rsp(req);
    rsp.set_body_file(req.get_path(), get_file_content_type(req.get_path()));
    return rsp;
}

HttpResponse def_err_handler(HttpCode code, const HttpRequest &req)
{
    HttpResponse rsp(req);
    rsp.set_code(code);
    rsp.header_oper(HttpResponse::HeaderOper::MODIFY,
                    "Connection", "close");

    std::string err_body1 = "<!DOCTYPE html>\r\n"
                           "<html>\r\n"
                           "<head><title>Lite Web Server</title></head>\r\n"
                           "<body><h1>";
    std::string err_body2 = std::string(http_enum_to_str<HttpCode>(code)) + ".</h1></body>\r\n</html>\r\n";
    rsp.set_body_bin(err_body1 + err_body2, HttpContentType::HTML_TYPE);

    return rsp;
}

HttpResponse err_handler_400(const HttpRequest &req)
{
    return def_err_handler(HttpCode::BAD_REQUEST, req);
}

HttpResponse err_handler_404(const HttpRequest &req)
{
    return def_err_handler(HttpCode::NOT_FOUND, req);
}

HttpResponse err_handler_405(const HttpRequest &req)
{
    return def_err_handler(HttpCode::NOT_ALLOWED, req);
}

HttpResponse err_handler_500(const HttpRequest &req)
{
    return def_err_handler(HttpCode::INTERNAL_SERVER_ERROR, req);
}
