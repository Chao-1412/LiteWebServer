#ifndef SRC_HTTPDATA_H_
#define SRC_HTTPDATA_H_

#include <unordered_map>
#include <string>

#include <stdint.h>

#include "stringutil.h"


template <typename EnumType> LWS_CONSTEXPR const char* http_enum_to_str(EnumType e);
template <typename EnumType> LWS_CONSTEXPR EnumType http_str_to_enum(const char* str);

// new HttpMethod enum insert here
#define HTTPMETHOD_ENUM \
    X(UNKNOWN)          \
    X(GET)              \
    X(POST)             \
    X(PUT)              \
    X(DELETE)           \
    X(HEAD)             \
    X(CONNECT)          \
    X(OPTIONS)          \
    X(TRACE)            \
    X(PATCH)

enum class HttpMethod
{
    #define X(NAME) NAME,
    HTTPMETHOD_ENUM
    #undef X
};

// new HttpCode enum insert here
#define HTTPCODE_ENUM                   \
    X(UNKNOWN, 0, "Unknown")            \
    X(OK, 200, "OK")                    \
    X(BAD_REQUEST, 400, "Bad Request")  \
    X(NOT_FOUND, 404, "Not Found")      \
    X(FORBIDDEN, 403, "Forbidden")      \
    X(NOT_ALLOWED, 405, "Method Not Allowed") \
    X(INTERNAL_SERVER_ERROR, 500, "Internal Server Error") \

enum class HttpCode
{
    #define X(NAME, CODE, DESC) NAME = CODE,
    HTTPCODE_ENUM
    #undef X
};

// new HttpVersion enum insert here
#define HTTPVERSION_ENUM \
    X(UNKNOWN, "Unknown")          \
    X(HTTP_1_0, "HTTP/1.0")         \
    X(HTTP_1_1, "HTTP/1.1")         \

enum class HttpVersion
{
    #define X(NAME, DESC) NAME,
    HTTPVERSION_ENUM
    #undef X
};

// new HttpContentType enum insert here
#define HTTPCONTENTTYPE_ENUM \
    X(UNKNOWN, 0x00, "")   \
    X(HTML_TYPE, 0x01, "text/html")   \
    X(JSON_TYPE, 0x02, "application/json")   \

enum class HttpContentType
{
    #define X(NAME, CODE, DESC) NAME = CODE,
    HTTPCONTENTTYPE_ENUM
    #undef X
};

template<>
LWS_CONSTEXPR const char* http_enum_to_str<HttpMethod>(HttpMethod e)
{
    switch (e) {
        #define X(NAME) case HttpMethod::NAME: return #NAME;
        HTTPMETHOD_ENUM
        #undef X
        default: return "";
    }
}

template<>
LWS_CONSTEXPR HttpMethod http_str_to_enum<HttpMethod>(const char* str)
{
    #define X(NAME) if (StringUtil::ch_str_is_equal(str, #NAME)) return HttpMethod::NAME;
    HTTPMETHOD_ENUM
    #undef X
    return HttpMethod::UNKNOWN;
}

template<>
LWS_CONSTEXPR const char* http_enum_to_str<HttpCode>(HttpCode e)
{
    switch (e) {
        #define X(NAME, CODE, DESC) case HttpCode::NAME: return DESC;
        HTTPCODE_ENUM
        #undef X
        default: return "";
    }
}

template<>
LWS_CONSTEXPR HttpCode http_str_to_enum<HttpCode>(const char* str)
{
    #define X(NAME, CODE, DESC) if (StringUtil::ch_str_is_equal(str, DESC)) return HttpCode::NAME;
    HTTPCODE_ENUM
    #undef X
    return HttpCode::UNKNOWN;
}

template<>
LWS_CONSTEXPR const char* http_enum_to_str<HttpVersion>(HttpVersion e)
{
    switch (e) {
        #define X(NAME, DESC) case HttpVersion::NAME: return DESC;
        HTTPVERSION_ENUM
        #undef X
        default: return "";
    }
}

template<>
LWS_CONSTEXPR HttpVersion http_str_to_enum<HttpVersion>(const char* str)
{
    #define X(NAME, DESC) if (StringUtil::ch_str_is_equal(str, DESC)) return HttpVersion::NAME;
    HTTPVERSION_ENUM
    #undef X
    return HttpVersion::UNKNOWN;
}

template<>
LWS_CONSTEXPR const char* http_enum_to_str<HttpContentType>(HttpContentType e)
{
    switch (e) {
        #define X(NAME, CODE, DESC) case HttpContentType::NAME: return DESC;
        HTTPCONTENTTYPE_ENUM
        #undef X
        default: return "";
    }
}

template<>
LWS_CONSTEXPR HttpContentType http_str_to_enum<HttpContentType>(const char* str)
{
    #define X(NAME, CODE, DESC) if (StringUtil::ch_str_is_equal(str, DESC)) return HttpContentType::NAME;
    HTTPCONTENTTYPE_ENUM
    #undef X
    return HttpContentType::UNKNOWN;
}


class HttpRequest
{
private:
    enum class ParseState
    {
        PARSE_REQ_LINE = 0x01,
        PARSE_REQ_HEADER = 0x02,
        PARSE_BODY = 0x04,
        PARSE_SUCCESS = 0x08,
    };

public:
    HttpRequest();
    /**
     *TODO 可能需要优化解析性能,
     * @brief 解析HTTP请求, 
     *        调用者要自己保证start_idx参数是合法的, 否则可能造成未知的后果
     *        只要解析到错误数据就返回，不管后续数据, 
     *        通过调用parse_complete()判断解析是否成功, 
     *        通过调用is_bad_req()判断是否是非法请求
     * @param data 待解析的HTTP请求数据
     * @param start_idx 解析起始位置
     * @return 返回从data中已解析出的字节数，不一定等于data.size()，因为：
     *         1. 可能解析到\r\n\r\n，说明请求头解析完毕，后续数据不再解析
     *         2. 可能数据量不足，需要等待下次调用
     *         下次调用可以传递给start_idx参数，加速解析
     */
    uint32_t parse(const std::string &data, uint32_t start_idx = 0);
    /**
     * @brief 判断是否解析完毕
     * @return true 解析完毕
     * @return false 解析未完毕
     */
    bool parse_complete() const { return state_ == ParseState::PARSE_SUCCESS; }
    /**
     * @brief 判断是否是非法请求，
     *        使用前需要先调用parse_complete来判断是否解析完毕
     *        否则永远是false
     * @return true 是非法请求
     * @return false 不是非法请求
     */
    bool is_bad_req() const { return is_bad_req_; }
    HttpMethod get_method() const { return method_; }
    std::string get_path() const { return path_; }
    std::string get_http_ver() const { return http_ver_; }
    /**
     * @brief 获取请求头
     * @param val 输出参数值
     * @param key 参数名
     * @return true 获取成功
     * @return false 获取失败
     */
    bool get_header(const std::string &key, std::string &val) const;
    /**
     * @brief 获取请求参数
     * @param val 输出参数值
     * @param key 参数名
     * @return true 获取成功
     * @return false 获取失败
     */
    bool get_param(const std::string &key, std::string &val) const;
    //TODO 添加解析body的方法
    void reset() {
        state_ = ParseState::PARSE_REQ_LINE;
        is_bad_req_ = false;
        method_ = HttpMethod::UNKNOWN;
        path_.clear();
        http_ver_.clear();
        headers_.clear();
        param_.clear();
        body_.clear();
    }
    const std::string& get_body() const { return body_; }
    void dump_data();
    std::string dump_data_str();

private:
    void set_bad_req() { is_bad_req_ = true; state_ = ParseState::PARSE_SUCCESS; }
    //TODO 需要优化
    uint32_t parse_req_line(const std::string &data, uint32_t start_idx);
    bool parse_uri(const std::string &uri);
    bool path_is_vaild(const std::string &path);
    uint32_t parse_req_header(const std::string &data, uint32_t start_idx);
    uint32_t parse_req_body(const std::string &data, uint32_t start_idx);

private:
    ParseState state_;
    bool is_bad_req_;
    HttpMethod method_;
    std::string path_;
    std::string http_ver_;
    //TODO 使用shared_ptr优化内存和执行效率！
    //TODO unordered_map or map?
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> param_;
    std::string body_;
};

class HttpResponse
{
public:
    enum class HeaderOper {
        ADD,
        DEL,
        MODIFY,
        CLEAR
    };

public:
    HttpResponse(const HttpRequest &req);

public:
    void set_code(HttpCode code) { maked_base_rsp_ = false; code_ = code; }
    /**
     * @brief 设置响应头
     * @param oper 操作类型
     * @param key 键
     * @param val 值，DEL操作时，val可以为空或不传递
     */
    void header_oper(HeaderOper oper, const std::string &key, const std::string &val);
    bool get_header(const std::string &key, std::string &val) const;
    /**
     * @brief 设置响应体
     * @param type 响应体类型
     * @param data 响应体数据
     * @param is_file 是否是文件，true表示是文件，false表示二进制string流
     *                默认是文件类型，因为正常都是从文件读数据的！所以要用二进制流请手动设置
     */
    void set_body(HttpContentType type, const std::string &data, bool is_file = true);
    HttpContentType get_body_type() const { return body_type_; }
    bool body_is_file() const { return body_is_file_; }
    const std::string& get_body() const { return body_; }
    const std::string& get_base_rsp() {
        if (!maked_base_rsp_) { make_base_rsp(); }
        return base_rsp_;
    }
    void reset() {
        http_ver_.clear();
        code_ = HttpCode::OK;
        headers_.clear();
        headers_.emplace("Content-Type", "text/html; charset=UTF-8");
        headers_.emplace("Connection", "close");
        maked_base_rsp_ = false;
        base_rsp_.clear();
        body_.clear();
        body_type_ = HttpContentType::HTML_TYPE;
        body_is_file_ = true;
    }
    void dump_data();
    std::string dump_data_str();

private:
    void make_base_rsp();

private:
    std::string http_ver_;
    HttpCode code_;
    std::unordered_map<std::string, std::string> headers_;
    bool maked_base_rsp_;
    std::string base_rsp_;
    std::string body_;
    HttpContentType body_type_;
    bool body_is_file_;
};

HttpResponse def_err_handler(HttpCode code, const HttpRequest &req);
HttpResponse err_handler_400(const HttpRequest &req);
HttpResponse err_handler_404(const HttpRequest &req);
HttpResponse err_handler_405(const HttpRequest &req);
HttpResponse err_handler_500(const HttpRequest &req);

#endif //SRC_HTTPDATA_H_
