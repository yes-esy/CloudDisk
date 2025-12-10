/**
 * @FilePath     : /CloudDisk/include/common/Net.h
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 21:10:44
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "Common.h"
#define EPOLL_ARR_SIZE 100
ssize_t recvn(int socketFd, void *buff, size_t len);
ssize_t sendn(int socketFd, const void *buff, size_t len);
int tcpInit(const char *ip, const char *port);
int addEpollReadFd(int epollFd, int fd);
int delEpollReadfd(int epollFd, int fd);
int tcpConnect(const char *ip, unsigned short port) ;
