#include "httpdata.h"

#include <vector>

#include <stdint.h>

#include "stringutil.h"


HttpRequest::HttpRequest()
    : state_(ParseState::PARSE_REQ_LINE)
    , is_bad_req_(false)
    , method_(HttpMethod::UNKNOWN)
    , path_("")
    , http_ver_("")
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
    if (state_ == ParseState::PARSE_SUCCESS) {
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

bool HttpRequest::parse_complete() const
{
    return state_ == ParseState::PARSE_SUCCESS;
}

bool HttpRequest::is_bad_req() const
{
    return is_bad_req_;
}

HttpMethod HttpRequest::get_method() const
{
    return method_;
}

std::string HttpRequest::get_path() const
{
    return path_;
}

void HttpRequest::set_bad_req()
{
    is_bad_req_ = true;
    state_ = ParseState::PARSE_SUCCESS;
}

uint32_t HttpRequest::parse_req_line(const std::string &data, uint32_t start_idx)
{
    // 获取 请求行
    std::string line = "";
    std::size_t line_end = std::string::npos;

    line_end = data.find("\r\n", start_idx);
    if (line_end == std::string::npos) {
        set_bad_req();
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
    HttpMethod method = http_str_to_enum<HttpMethod>(segment.c_str());
    if (method == HttpMethod::UNKNOWN) {
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
    // std::string line;

    // while (1) {
    //     // 按行分割
    //     std::size_t pos = data.find("\r\n", start_idx);
    //     if (pos == std::string::npos) {
    //         state_ = ParseState::PARSE_PROCESSING;
    //         return parsed_bytes;
    //     }

    //     if (pos == start_idx) {
    //         // 空行，只能说明请求头解析完毕
    //         // 还不能直接返回，要根据请求类型判断是否还有数据段，是否请求成功
    //         // 比如POST请求，请求头后面还有数据段
    //         state_ = ParseState::PARSE_SUCCESS;
    //         break;
    //     }

    //     line = data.substr(start_idx, pos - start_idx);
    //     parse_req_line(line);
    //     parse_req_header(line);
    // }

    return 0;
}

uint32_t HttpRequest::parse_req_body(const std::string &data, uint32_t start_idx)
{
    return 0;
}
