/**
 * @FilePath     : /CloudDisk/include/client/Client.h
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-11 22:23:30
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include "Common.h"
int processStdin(char *buf, size_t buflen);
int processCommand(int clientFd, packet_t *packet, char *buf);
int processServer(int clientFd,char * buf,int bufLen);
int processClient(int clientFd, fd_set * rdset);