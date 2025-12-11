/**
 * @FilePath     : /CloudDisk/src/client/Client.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-11 22:25:14
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Client.h"
#include "unistd.h"
#include "Net.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
/**
 * @brief        : 处理输入
 * @param         {char} *buf: 输入缓冲区
 * @param         {size_t} buflen: 缓冲区长度
 * @return        {int} : 状态码>0成功获得输入,==0忽略(全空白),<0 出错或EOF
**/
int processStdin(char *buf, size_t buflen) {
    if (!buf || buflen == 0)
        return -1;
    buf[0] = '\0';
    ssize_t ret = read(STDIN_FILENO, buf, buflen - 1);
    if (ret <= 0) {
        if (ret < 0)
            perror("read stdin");
        if (ret == 0)
            printf("byebye.\n");
        return (int)ret;
    }
    /* 去掉末尾换行（如果有）并保证不越界 */
    if (ret > 0 && buf[ret - 1] == '\n') {
        buf[ret - 1] = '\0';
    } else {
        buf[ret] = '\0';
    }
    {
        int only_space = 1;
        for (size_t i = 0; buf[i] != '\0'; ++i) {
            if (!isspace((unsigned char)buf[i])) {
                only_space = 0;
                break;
            }
        }
        if (only_space) {
            return 0;
        }
    }
    return (int)ret;
}
/**
 * @brief        : 处理命令发送
 * @param         {int} clientFd: client socket
 * @param         {packet_t} *packet: 数据包
 * @param         {char} *buf: 缓冲区
 * @return        {int} 状态码 <0 错误,
**/
int processCommand(int clientFd, packet_t *packet, char *buf) {
    if (!packet || !buf) {
        perror("packet or buf is NULL");
        return -1;
    }
    memset(packet, 0, sizeof(packet_t));
    parseCommand(buf, strlen(buf), packet);
    if (sendn(clientFd, packet, 4 + 4 + packet->len) < 0) {
        perror("sendn");
        return -1;
    }
    if (packet->cmdType == CMD_TYPE_PUTS) {
    }
    return 0;
}
/**
 * @brief        : 处理服务器的事件
 * @param         {int} clientFd: 客户端socket
 * @param         {char} *buf: 接收缓冲区
 * @param         {int} bufLen: 缓冲区长度
 * @return        {int} 状态码<0错误
**/
int processServer(int clientFd, char *buf, int bufLen) {
    if (!buf || bufLen <= 0) {
        return -1;
    }
    int ret = recv(clientFd, buf, bufLen - 1, 0);
    if (ret < 0) {
        perror("recv");
    } else if (ret == 0) {
        printf("server closed connection\n");
    } else {
        buf[ret] = '\0';
        printf("recv:%s\n", buf);
    }
    return ret;
}
/**
 * @brief        : 处理客户端
 * @param         {int} clientFd: 客户端socket
 * @param         {fd_set} *rdset: 
 * @return        {*}
**/
int processClient(int clientFd, fd_set *rdset) {
    FD_ZERO(rdset);
    FD_SET(STDIN_FILENO, rdset);
    FD_SET(clientFd, rdset);

    int nready = select(clientFd + 1, rdset, NULL, NULL, NULL);
    if (nready < 0) {
        perror("select");
    }
    return nready;
}