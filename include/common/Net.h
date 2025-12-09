/**
 * @FilePath     : /CloudDisk/include/Net.h
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-08 22:42:43
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "Common.h"
#define EPOLL_ARR_SIZE 100
int tcpInit(const char *ip, const char *port);
int addEpollReadFd(int epollFd, int fd);
