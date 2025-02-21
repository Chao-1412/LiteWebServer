#include <cstdio>
#include <cstring>
#include <map>
#include <iostream>

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "litewebserver.h"
#include "userconn.h"
#include "httpdata.h"
#include "debughelper.h"


// static const std::string index_body = "<!DOCTYPE html>\r\n"
//                                       "<html>\r\n"
//                                       "<head><title>Simple Web Server</title></head>\r\n"
//                                       "<body><h1>Hello, World! This is a simple web server.</h1></body>\r\n"
//                                       "</html>\r\n";
// HttpResponse index_page(const HttpRequest &req)
// {
//     HttpResponse rsp(req);
//     rsp.set_body_bin(index_body, HttpContentType::HTML_TYPE);
//     return rsp;
// }

// HttpResponse hello_page(const HttpRequest &req)
// {
//     HttpResponse rsp(req);
//     rsp.set_body_file("hello.html", HttpContentType::HTML_TYPE);
//     return rsp;
// }

// HttpResponse favicon_ico(const HttpRequest &req)
// {
//     HttpResponse rsp(req);
//     rsp.set_body_file("favicon.ico", HttpContentType::XICON_TYPE);
//     return rsp;
// }


int main()
{
    /**
     * @brief 测试header_oper的接口性能
     */
    // HttpRequest req;
    // HttpResponse rsp(req);
    // long long unsigned int times = 10000000;
    // // 如果不是要临时分配的话，引用的性能就很不错，没必要特意处理成移动版本
    // std::string key = "Content-Type";
    // std::string value = "text/html";

    // {
    //     TimeCount tc("add header lref", times);
    //     for (long long unsigned i = 0; i < times; ++i) {
    //         // 临时分配的话性能就会差点，不如直接原地构造右值
    //         // std::string key = "Content-Type";
    //         // std::string value = "text/html";
    //         rsp.header_oper(HttpResponse::HeaderOper::ADD,
    //                         key, value);
    //     }
    // }

    // {
    //     TimeCount tc("add header rref", times);
    //     for (long long unsigned i = 0; i < times; ++i) {
    //         rsp.header_oper(HttpResponse::HeaderOper::ADD,
    //                         "Content-Type", "text/html");
    //     }
    // }

    // UserConn::register_router("/", HttpMethod::GET, index_page);
    // UserConn::register_router("/favicon.ico", HttpMethod::GET, favicon_ico);
    // UserConn::register_router("/hello", HttpMethod::GET, hello_page);
    // UserConn::register_router("/json", HttpMethod::GET,
    //     [](const HttpRequest &req) -> HttpResponse {
    //         HttpResponse rsp(req);
    //         std::string json_data = "{\"name\": \"John\", \"age\": 30}";
    //         rsp.set_body_bin(json_data, HttpContentType::JSON_TYPE);
    //         return rsp;
    //     }
    // );
    
    ServerConf conf(8080, "../testsite");
    LiteWebServer server(conf);
    server.start_loop();

    return 0;
 }
