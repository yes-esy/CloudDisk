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
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_GREEN "\033[32m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"
#define COLOR_YELLOW "\033[33m"
extern char username[USERNAME_LENGTH];
extern char workforlder[PATH_MAX_LENGTH];


/**
 * @brief : 用户登录 
 * @param sockfd:
 * @return void
**/
void userLogin(int sockfd);
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
/**
 * @brief 打印命令提示符 - Bash风格
 * 格式: [username@CloudDisk pwd]$ 
 * 示例: [yes@CloudDisk /data]$ 
 */
static inline void print_prompt() {
    if (username[0] && workforlder[0]) {
        // 登录后: [用户名@CloudDisk 当前目录]$
        printf("%s[%s%s@CloudDisk%s %s%s%s]$%s ", COLOR_BOLD, COLOR_GREEN, username,
               COLOR_RESET COLOR_BOLD, COLOR_BLUE, workforlder, COLOR_RESET COLOR_BOLD,
               COLOR_RESET);
    } else {
        // 登录前: CloudDisk>
        printf("%sCloudDisk>%s ", COLOR_CYAN, COLOR_RESET);
    }
    fflush(stdout);
}

/**
 * @brief 打印欢迎信息
 */
static inline void print_welcome() {
    printf("%s", COLOR_CYAN);
    printf("╔════════════════════════════════════════╗\n");
    printf("║   Welcome to CloudDisk Client v0.2.0   ║\n");
    printf("║   Type 'help' for available commands   ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("%s", COLOR_RESET);
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
