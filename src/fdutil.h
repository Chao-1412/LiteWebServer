#ifndef SRC_FD_UTIL_
#define SRC_FD_UTIL_

#include <stdint.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/tcp.h> 
#include <netinet/ip.h>


class FdUtil
{
public:
    static int set_nonblocking(int fd);
    static int epoll_add_fd(int epollfd, int fd, uint32_t events);
    /**
     * @brief 添加fd到epollfd，默认设置为EPOLLONESHOT
     * @param epollfd epoll实例
     * @param fd 需要添加的fd
     * @param events 事件类型
     * @return epoll_ctl的返回值，详细错误和epoll_ctl一致通过errno获取
     */
    static int epoll_add_fd_oneshot(int epollfd, int fd, uint32_t events);
    /**
     * @brief 修改fd在epollfd中的事件类型，默认设置为EPOLLONESHOT
     * 通过EPOLL_CTL_ADD命令添加的带有EPOLLONESHOT标志的fd，在触发一次后不再触发，
     * 需要EPOLL_CTL_DEL后重新EPOLL_CTL_ADD
     * 或者通过EPOLL_CTL_MOD修改
     * @param epollfd epoll实例
     * @param fd 需要添加的fd
     * @param events 事件类型
     * @return epoll_ctl的返回值，详细错误和epoll_ctl一致通过errno获取
     */
    static int epoll_mod_fd_oneshot(int epollfd, int fd, uint32_t events);
    static int epoll_del_fd(int epollfd, int fd);
    /**
     * @brief 关闭Nagle
     *        Nagle 算法，可能导致小数据包延迟发送
     *        在实时性要求高的应用中可以关闭
     */
    static int set_socket_nodelay(int fd);
    static int set_socket_reuseaddr(int fd);
};

inline int FdUtil::set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) { return -1; }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

inline int FdUtil::epoll_add_fd(int epollfd, int fd, uint32_t events)
{
    //TODO 使用thread_local优化每次的变量创建
    struct epoll_event sock_event;
    sock_event.data.fd = fd;
    sock_event.events = events;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &sock_event);
} 

inline int FdUtil::epoll_add_fd_oneshot(int epollfd, int fd, uint32_t events) {
    struct epoll_event sock_event;
    sock_event.data.fd = fd;
    sock_event.events = events | EPOLLONESHOT;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &sock_event);
}

inline int FdUtil::epoll_mod_fd_oneshot(int epollfd, int fd, uint32_t events)
{
    struct epoll_event sock_event;
    sock_event.data.fd = fd;
    sock_event.events = events | EPOLLONESHOT;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &sock_event);
}

inline int FdUtil::epoll_del_fd(int epollfd, int fd)
{
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

inline int FdUtil::set_socket_nodelay(int fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flag, sizeof(flag));
}

inline int FdUtil::set_socket_reuseaddr(int fd)
{
    int flag = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

#endif // SRC_FD_UTIL_
