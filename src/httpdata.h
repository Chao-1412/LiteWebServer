#ifndef HTTPDATA_H_
#define HTTPDATA_H_

#include <unordered_map>
#include <string>

#include <stdint.h>

#include "stringutil.h"


template <typename EnumType> constexpr const char* http_enum_to_str(EnumType e);
template <typename EnumType> constexpr EnumType http_str_to_enum(const char* str);

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

template<>
constexpr const char* http_enum_to_str<HttpMethod>(HttpMethod e)
{
    switch (e) {
        #define X(NAME) case HttpMethod::NAME: return #NAME;
        HTTPMETHOD_ENUM
        #undef X
        default: return "";
    }
}

template<>
constexpr HttpMethod http_str_to_enum<HttpMethod>(const char* str)
{
    #define X(NAME) if (StringUtil::ch_str_is_equal(str, #NAME)) return HttpMethod::NAME;
    HTTPMETHOD_ENUM
    #undef X
    return HttpMethod::UNKNOWN;
}

template<>
constexpr const char* http_enum_to_str<HttpCode>(HttpCode e)
{
    switch (e) {
        #define X(NAME, CODE, DESC) case HttpCode::NAME: return #DESC;
        HTTPCODE_ENUM
        #undef X
        default: return "";
    }
}

template<>
constexpr HttpCode http_str_to_enum<HttpCode>(const char* str)
{
    #define X(NAME, CODE, DESC) if (StringUtil::ch_str_is_equal(str, #NAME)) return HttpCode::NAME;
    HTTPCODE_ENUM
    #undef X
    return HttpCode::UNKNOWN;
}

template<>
constexpr const char* http_enum_to_str<HttpVersion>(HttpVersion e)
{
    switch (e) {
        #define X(NAME, DESC) case HttpVersion::NAME: return #DESC;
        HTTPVERSION_ENUM
        #undef X
        default: return "";
    }
}

template<>
constexpr HttpVersion http_str_to_enum<HttpVersion>(const char* str)
{
    #define X(NAME, DESC) if (StringUtil::ch_str_is_equal(str, #NAME)) return HttpVersion::NAME;
    HTTPVERSION_ENUM
    #undef X
    return HttpVersion::UNKNOWN;
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
     *TODO 可能需要优化解析性能
     * @brief 解析HTTP请求
     *        通过\r\n分割数据
     *        只要解析到错误数据就返回，不管后续数据
     * @param data 待解析的HTTP请求数据
     * @param start_idx 解析起始位置
     * @return 返回从data中已解析出的字节数，不一定等于data.size()，因为：
     *         1. 可能解析到\r\n\r\n，说明请求头解析完毕，后续数据不再解析
     *         2. 可能数据量不足，需要等待下次调用
     *         下次调用可以传递给start_idx参数，加速解析
     */
    uint32_t parse(const std::string &data, uint32_t start_idx = 0);
    bool parse_complete() const;
    bool is_bad_req() const;
    HttpMethod get_method() const;
    std::string get_path() const;

private:
    void set_bad_req();
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
    //TODO 使用shared_ptr优化内存
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> param_;
    std::string body_;
};

class HttpResponse
{
};

#endif
