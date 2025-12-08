/**
 * @FilePath     : /CloudDisk/src/Common/Net.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-08 22:45:45
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Net.h"
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