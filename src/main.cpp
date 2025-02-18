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


static const std::string index_body = "<!DOCTYPE html>\r\n"
                                      "<html>\r\n"
                                      "<head><title>Simple Web Server</title></head>\r\n"
                                      "<body><h1>Hello, World! This is a simple web server.</h1></body>\r\n"
                                      "</html>\r\n";
HttpResponse index_page(const HttpRequest &req)
{
    HttpResponse rsp(req);
    rsp.set_body_bin(index_body, HttpContentType::HTML_TYPE);
    return rsp;
}

HttpResponse hello_page(const HttpRequest &req)
{
    HttpResponse rsp(req);
    rsp.set_body_file("hello.html", HttpContentType::HTML_TYPE);
    return rsp;
}

HttpResponse favicon_ico(const HttpRequest &req)
{
    HttpResponse rsp(req);
    rsp.set_body_file("favicon.ico", HttpContentType::XICON_TYPE);
    return rsp;
}


int main()
{
    UserConn::register_router("/", HttpMethod::GET, index_page);
    UserConn::register_router("/favicon.ico", HttpMethod::GET, favicon_ico);
    UserConn::register_router("/json", HttpMethod::GET,
        [](const HttpRequest &req) -> HttpResponse {
            HttpResponse rsp(req);
            std::string json_data = "{\"name\": \"John\", \"age\": 30}";
            rsp.set_body_bin(json_data, HttpContentType::JSON_TYPE);
            return rsp;
        }
    );
    UserConn::register_router("/hello", HttpMethod::GET, hello_page);

    ServerConf conf(8080, "../testsite");
    LiteWebServer server(conf);
    server.start_loop();

    return 0;
 }
