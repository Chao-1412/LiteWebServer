#include <cstdio>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static void print_errno_info(const char *const extra_msg)
{
    if (extra_msg) {
        printf("%s, no: %d, msg: %s\n", extra_msg, errno, strerror(errno));
    } else {
        printf("no: %d, msg: %s\n", errno, strerror(errno));
    }
}

static void handle_client(int client_sock)
{
    char buffer[1024];
    
    // 读取客户端请求
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        print_errno_info("recv failed");
        return;
    }
    buffer[bytes_received] = '\0';  // 确保字符串以 null 结尾

    printf("Request received:\n%s\n", buffer);

    // 发送 HTTP 响应
    const char *http_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html>\r\n"
        "<html>\r\n"
        "<head><title>Simple Web Server</title></head>\r\n"
        "<body><h1>Hello, World! This is a simple web server.</h1></body>\r\n"
        "</html>\r\n";
    
    send(client_sock, http_response, strlen(http_response), 0);
}


int main()
{
    int ret = 0;
    int srv_sock = -1;
    int sockopt_reuse = 1;

    uint16_t listen_port = 8080;
    struct sockaddr_in host_addr;
    socklen_t host_addr_size = sizeof(host_addr);
    memset(&host_addr, 0, host_addr_size);
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    host_addr.sin_port = htons(listen_port);

    int con_queue_n = 5;

    int cli_sock;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_size = 0;

    srv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (srv_sock <= 0) {
        print_errno_info("create socket failed");
        ret = errno;
        goto EXITED;
    }
    setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &sockopt_reuse, sizeof(sockopt_reuse));

    ret = bind(srv_sock, (struct sockaddr*)&host_addr, host_addr_size);
    if (ret < 0) {
        print_errno_info("bind failed");
        ret = errno;
        goto EXITED;
    }
    printf("Server bound to port %d\n", listen_port);

    ret = listen(srv_sock, con_queue_n);
     if (ret < 0) {
        print_errno_info("listen failed");
        ret = errno;
        goto EXITED;
    }
    printf("Server is listening...\n");

    while (1) {
        cli_sock = accept(srv_sock, (struct sockaddr *)&cli_addr, &cli_addr_size);
        if (cli_sock < 0) {
            print_errno_info("accept client failed");
            continue;
        }

        printf("Recive client connect, cli_sock: %d, ip: %s\n",
               cli_sock, inet_ntoa(cli_addr.sin_addr));

        // 处理客户端请求
        handle_client(cli_sock);
        close(cli_sock);
    }


EXITED:
    if (srv_sock) {
        close(srv_sock);
    }

    return ret;
}
