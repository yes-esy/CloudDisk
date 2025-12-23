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
#include <sys/stat.h>
#include "log.h"
#include <string.h>
#include "tools.h"
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
 * @brief 格式化文件大小显示（优化版，固定格式）
 * @param bytes 字节数
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
static void formatFileSize(uint32_t bytes, char *buffer, size_t size) {
    if (bytes >= 1024 * 1024 * 1024) {
        // GB级别
        double gb = (double)bytes / (1024 * 1024 * 1024);
        snprintf(buffer, size, "%.1f GB", gb);
    } else if (bytes >= 1024 * 1024) {
        // MB级别
        double mb = (double)bytes / (1024 * 1024);
        snprintf(buffer, size, "%.1f MB", mb);
    } else if (bytes >= 1024) {
        // KB级别
        double kb = (double)bytes / 1024;
        snprintf(buffer, size, "%.1f KB", kb);
    } else {
        // Bytes级别
        snprintf(buffer, size, "%u B", bytes);
    }
}

/**
 * @brief 显示美化的进度条（固定长度，无闪烁）
 * @param current 当前进度
 * @param total 总量
 * @param prefix 前缀文本
 */
static void showProgressBar(uint32_t current, uint32_t total, const char *prefix) {
    const int barWidth = 50; // 固定进度条宽度
    double progress = (double)current / total;
    int pos = (int)(barWidth * progress);

    // 清除当前行
    printf("\r\033[K");

    // 前缀（固定宽度）
    printf("%-12s ", prefix ? prefix : "Progress");

    // 进度条边框
    printf("[");

    // 进度条内容（固定50字符宽度）
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) {
            printf("\033[42m \033[0m"); // 绿色背景
        } else {
            printf(" "); // 空格
        }
    }

    printf("] ");

    // 百分比（固定6字符宽度：xxx.x%）
    printf("%5.1f%%", progress * 100.0);

    // 文件大小信息（格式化为固定宽度）
    char currentStr[16], totalStr[16];
    formatFileSize(current, currentStr, sizeof(currentStr));
    formatFileSize(total, totalStr, sizeof(totalStr));

    // 固定宽度的大小显示（每个大小字段预留8字符）
    printf(" (%8s/%8s)", currentStr, totalStr);

    fflush(stdout);
}

/**
 * @brief 获取本地文件信息
 */
typedef struct {
    uint32_t fileSize;
    char md5Checksum[33];
    char filename[FILENAME_LENGTH];
} LocalFileInfo;

static int getLocalFileInfo(const char *filepath, LocalFileInfo *info) {
    struct stat st;
    if (stat(filepath, &st) < 0) {
        log_error("Failed to stat file: %s", filepath);
        return -1;
    }

    info->fileSize = (uint32_t)st.st_size;

    if (calculateFileMD5(filepath, info->md5Checksum) < 0) {
        log_error("Failed to calculate file checksum");
        return -1;
    }

    const char *filename = strrchr(filepath, '/');
    strncpy(info->filename, filename ? filename + 1 : filepath, sizeof(info->filename) - 1);

    return 0;
}

/**
 * @brief 检查本地部分文件
 */
static uint32_t getLocalPartialFileSize(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return (uint32_t)st.st_size;
    }
    return 0;
}

/**
 * @brief : 用户登录 
 * @param sockfd:
 * @return void
**/
void userLogin(int sockfd);
void userRegister(int sockfd);
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

int processResponse(int clientFd, const char *mode);
#endif /* CLOUDDISK_CLIENT_H */
