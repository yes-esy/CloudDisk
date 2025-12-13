/**
 * @FilePath     : /CloudDisk/src/common/net.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-13 21:21:48
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "net.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
/**
 * @brief        : 建立socket
 * @param         {char} *ip: ip地址
 * @param         {char} *port: 端口号
 * @return        {int} : 返回一个socket
**/
int tcpInit(const char *ip, const char *port) {
    // 创建socket
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        perror("socket");
        return -1;
    }
    int on = 1;
    int ret = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret < 0) {
        perror("setsockopt");
        return -1;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(port));
    address.sin_addr.s_addr = inet_addr(ip);
    ret = bind(socketFd, (const struct sockaddr *)&address, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("bind");
        close(socketFd); //关闭套接字
        return -1;
    }
    ret = listen(socketFd, 10);
    if (ret < 0) {
        perror("listen");
        close(socketFd);
        return -1;
    }
    return socketFd;
}
/**
 * @brief        : 将一个文件描述符添加到epoll实例中
 * @param         {int} epollFd: 具体的epollFd对应的实例
 * @param         {int} fd: 文件描述符
 * @return        {int} : 返回0
**/
int addEpollReadFd(int epollFd, int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}
/**
 * @brief        : 将一个文件描述符从epoll实例中删除
 * @param         {int} epollFd: 具体的epollFd对应的实例
 * @param         {int} fd: 文件描述符
 * @return        {int} : 返回0
**/
int delEpollReadfd(int epollFd, int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &ev);
    ERROR_CHECK(ret, -1, "epoll_ctl");
    return 0;
}
/**
 * @brief        : 确定发送len字节的数据
 * @param         {int} socketFd: 通信的socketFd
 * @param         {void *} buff: 发送数据的指针
 * @param         {int} len: 长度
 * @return        {ssize_t} : 发送的实际长度
**/
ssize_t sendn(int socketFd, const void *buff, size_t len) {
    size_t left = len;
    const char *p = buff;
    while (left > 0) {
        ssize_t ret = send(socketFd, p, left, 0);
        if (ret > 0) {
            left -= ret;
            p += ret;
            continue;
        }
        if (ret == 0) {
            /* 对端关闭连接 */
            return (ssize_t)(len - left);
        }
        /* ret < 0 */
        if (errno == EINTR) {
            /* 被信号中断，重试 */
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* 非阻塞 socket 暂时无法发送，调用者可选择等待可写后重试 */
            return (ssize_t)(len - left);
        }
        perror("send");
        return -1;
    }
    return (ssize_t)len;
}

/**
 * @brief        : 确保把 从socketFd接收到len字节的数据到buff中,避免短发送。
 * @param         {int} socketFd:
 * @param         {void} *buff:
 * @param         {size_t} len:
 * @return        {*}
**/
ssize_t recvn(int socketFd, void *buff, size_t len) {
    size_t left = len;
    char *p = buff;
    ssize_t ret = 0;
    while (left > 0) {
        ret = recv(socketFd, p, left, 0);
        if (ret > 0) {
            left -= ret;
            p += ret;
            continue;
        }
        if (ret == 0) {
            /* 对端关闭连接，返回已接收字节数 */
            return (ssize_t)(len - left);
        }
        /* ret < 0 */
        if (errno == EINTR) {
            /* 被信号中断，重试 */
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* 非阻塞 socket 暂时无数据，返回已接收字节数 */
            return (ssize_t)(len - left);
        }
        perror("recv");
        return -1;
    }
    return ret;
}
/**
 * @brief        : 创建TCP连接
 * @param         {char} *ip: ip地址
 * @param         {unsigned short} port: 端口
 * @return        {int} : 状态码1成功,其他失败
**/
int tcpConnect(const char *ip, unsigned short port) {
    //1. 创建TCP的客户端套接字
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd < 0) {
        perror("socket");
        return -1;
    }

    //2. 指定服务器的网络地址
    struct sockaddr_in serveraddr;
    //初始化操作,防止内部有脏数据
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;   //指定IPv4
    serveraddr.sin_port = htons(port); //指定端口号
    //指定IP地址
    serveraddr.sin_addr.s_addr = inet_addr(ip);

    //3. 发起建立连接的请求
    int ret = connect(clientfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (ret < 0) {
        perror("connect");
        close(clientfd);
        return -1;
    }
    return clientfd;
}