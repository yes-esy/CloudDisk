/**
 * @file client.h
 * @brief 客户端功能
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_CLIENT_H
#define CLOUDDISK_CLIENT_H

#include <sys/select.h>
#include <types.h>
#include <stdio.h>
#include "types.h"
#define PROMPT "CloudDisk> "

/**
 * @brief 发送请求给服务器
 * @param clientFd 客户端socket
 * @param packet 数据包
 * @return 成功返回发送字节数,失败返回-1
 */
int sendRequest(int clientFd, const packet_t *packet);
/**
 * @brief 接收服务器响应
 * @param clientFd 客户端socket
 * @param buf 接收缓冲区
 * @param bufLen 缓冲区大小
 * @param statusCode 输出:状态码
 * @param dataType 输出:数据类型
 * @return 成功返回数据长度,失败返回-1,连接关闭返回0
 */
int recvResponse(int clientFd, char *buf, int bufLen, ResponseStatus *statusCode,
                 DataType *dataType);
static void print_prompt() {
    printf("%s", PROMPT);
    fflush(stdout); // 立即刷新输出缓冲区
}

static void print_welcome() {
    printf("========================================\n");
    printf("   Welcome to CloudDisk Client v0.0.1  \n");
    printf("   Type 'help' for available commands  \n");
    printf("========================================\n");
    print_prompt();
}
/**
 * @brief 处理标准输入
 * @param buf 输入缓冲区
 * @param buflen 缓冲区长度
 * @return 状态码>0成功,==0忽略,<0出错或EOF
 */
int processStdin(char *buf, size_t buflen);

/**
 * @brief 处理命令发送
 * @param clientFd 客户端socket
 * @param packet 数据包
 * @param buf 缓冲区
 * @return 状态码<0错误
 */
int processCommand(int clientFd, packet_t *packet, char *buf);

/**
 * @brief 处理服务器事件
 * @param clientFd 客户端socket
 * @param buf 接收缓冲区
 * @param bufLen 缓冲区长度
 * @return 状态码<0错误
 */
int processServer(int clientFd, char *buf, int bufLen);

/**
 * @brief 处理客户端主循环
 * @param clientFd 客户端socket
 * @param rdset fd_set指针
 * @return 就绪的文件描述符数量
 */
int processClient(int clientFd, fd_set *rdset);

#endif /* CLOUDDISK_CLIENT_H */
