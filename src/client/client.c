/**
 * @FilePath     : /CloudDisk/src/client/client.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-21 21:39:40
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "client.h"
#include "net.h"
#include "protocol.h"
#include "log.h"
#include <unistd.h>
#include "tools.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
extern char workforlder[PATH_MAX_LENGTH];
/**
 * @brief 发送用户名并接收验证结果
 * @param sockfd socket描述符
 * @param packet_p 输出参数:服务器响应包
 * @return 成功返回0
 */
int sendUsername(int sockfd, packet_t *packet_p) {
    packet_t packet;
    // 接收服务器响应
    char responseBuf[RESPONSE_LENGTH] = {0};
    ResponseStatus statusCode;
    DataType dataType;
    while (1) {
        printf(USERNAME);
        char usernameInput[USERNAME_LENGTH] = {0};
        int ret = read(STDIN_FILENO, usernameInput, sizeof(usernameInput));
        if (ret <= 0) {
            printf("read error,try again,");
            continue;
        }
        usernameInput[ret - 1] = '\0'; // 去掉换行符
        memset(&packet, 0, sizeof(packet_t));
        packet.len = strlen(usernameInput);
        packet.cmdType = TASK_LOGIN_VERIFY_USERNAME;
        strcpy(packet.buff, usernameInput);
        ret = sendRequest(sockfd, &packet);
        if (ret <= 0) {
            printf("unknown error,try again\n");
            log_error("Failed to send username");
            continue;
        }

        log_info("send username: %s", usernameInput);
        log_debug("Waiting for username verification...");
        ret = recvResponse(sockfd, responseBuf, sizeof(responseBuf), &statusCode, &dataType);
        if (ret < 0) {
            log_error("Failed to receive response");
            continue;
        }
        if (ret == 0) {
            log_error("Server closed connection");
            return -1;
        }

        if (statusCode == STATUS_FAIL) {
            printf("%s", responseBuf);
            continue;
        }
        strncpy(username, usernameInput, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';

        memcpy(packet.buff, responseBuf, ret);
        packet.len = ret;

        break;
    }
    memcpy(packet_p, &packet, sizeof(packet_t));
    return 0;
}
/**
 * @brief 发送密码并接收验证结果
 * @param sockfd socket描述符
 * @param packet_p 输出参数:服务器响应包
 * @return 成功返回0
 */
int sendPassword(int sockfd, packet_t *packet_p) {
    packet_t packet;
    int ret;
    // 接收服务器响应
    char responseBuf[RESPONSE_LENGTH] = {0};
    ResponseStatus statusCode;
    DataType dataType;
    while (1) {
        char *password = getpass(PASSWORD);
        char *encrytped = crypt(password, packet_p->buff);
        packet.len = strlen(encrytped);
        packet.cmdType = TASK_LOGIN_VERIFY_PASSWORD;
        strncpy(packet.buff, encrytped, packet.len);
        ret = sendRequest(sockfd, &packet);
        if (ret <= 0) {
            printf("unknown error,try again\n");
            log_error("Failed to send username");
            continue;
        }
        log_info("send password ciphertext : %s", encrytped);
        log_debug("Waiting for password verification...");
        ret = recvResponse(sockfd, responseBuf, sizeof(responseBuf), &statusCode, &dataType);
        if (ret < 0) {
            log_error("Failed to receive response");
            continue;
        }
        if (ret == 0) {
            log_error("Server closed connection");
            return -1;
        }

        if (statusCode == STATUS_FAIL) {
            printf("%s", responseBuf);
            continue;
        }
        printf("%s", responseBuf); // "Login successful! Welcome, yes\n"

        // ✅ 第二次接收: 当前工作目录
        memset(workforlder, 0, sizeof(workforlder)); // 清空缓冲区
        ret = recvResponse(sockfd, workforlder, sizeof(workforlder), &statusCode, &dataType);

        if (ret < 0) {
            log_error("Failed to receive current directory");
            return -1;
        }

        if (ret == 0) {
            log_error("Server closed connection while receiving directory");
            return -1;
        }
        log_info("Login successful, current directory: %s", workforlder);
        break;
    }
    return 0;
}

/**
 * @brief : 用户登录 
 * @param sockfd: 客户端socket
 * @return void
**/
void userLogin(int sockfd) {
    int ret;
    packet_t packet;
    sendUsername(sockfd, &packet);
    sendPassword(sockfd, &packet);
}
/**
 * @brief 格式化文件大小显示（优化版，固定格式）
 * @param bytes 字节数
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
void formatFileSize(uint32_t bytes, char *buffer, size_t size) {
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
void showProgressBar(uint32_t current, uint32_t total, const char *prefix) {
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
 * @brief 从处理后的参数中提取源文件路径和目标路径
 * @param processedArgs 格式: "filename destpath"
 * @param srcPath 输出:原始源文件路径(从原始输入恢复)
 * @param destPath 输出:目标路径
 * @param originalInput 原始用户输入
 */
void parsePutsArgs(const char *processedArgs, const char *originalInput, char *srcPath,
                   char *destPath) {
    // processedArgs 格式: "filename destpath"
    // 需要从 originalInput 恢复完整源路径

    // 1. 从 processedArgs 提取 filename
    const char *space = strchr(processedArgs, ' ');
    if (!space) {
        srcPath[0] = '\0';
        destPath[0] = '\0';
        return;
    }

    // 2. 提取目标路径
    strcpy(destPath, space + 1);

    // 3. 从原始输入提取源路径(puts 后第一个参数)
    const char *input = originalInput;
    while (*input && isspace(*input))
        input++; // 跳过前导空格
    while (*input && !isspace(*input))
        input++; // 跳过 "puts"
    while (*input && isspace(*input))
        input++; // 跳过空格

    int i = 0;
    while (*input && !isspace(*input)) {
        srcPath[i++] = *input++;
    }
    srcPath[i] = '\0';
}
/**
 * @brief 解析参数，提取目标全路径和原始文件名
 * @param processedArgs 格式: "src_path/filename dest_path"
 * @param fileLocalPath 输出: "dest_path/filename"
 * @param fileRemotePath 输出: "src_path/filename"
 * @param fileName 输出: 提取出的文件名（不含路径）
 */
void parseGetsArgs(char *processedArgs, char *fileRemotePath, char *fileLocalPath, char *fileName) {
    if (!processedArgs || !fileLocalPath || !fileName) {
        if (fileLocalPath)
            fileLocalPath[0] = '\0';
        if (fileName)
            fileName[0] = '\0';
        if (fileRemotePath)
            fileRemotePath[0] = '\0';
        return;
    }
    char *space = strchr(processedArgs, ' ');
    if (!space) {
        fileLocalPath[0] = '\0';
        fileName[0] = '\0';
        fileRemotePath[0] = '\0';
        return;
    }
    size_t remote_len = space - processedArgs;
    memcpy(fileRemotePath, processedArgs, remote_len);
    fileRemotePath[remote_len] = '\0';
    // 检查源路径是否为空（如 " dest"）
    if (space == processedArgs) {
        fileLocalPath[0] = '\0';
        fileName[0] = '\0';
        fileRemotePath[0] = '\0';
        return;
    }
    // 处理目标路径
    const char *dest = space + 1;
    size_t dest_len = strlen(dest);
    if (dest_len >= FILENAME_MAX) {
        fileLocalPath[0] = '\0';
        fileName[0] = '\0';
        return;
    }
    char destinationPath[FILENAME_MAX];
    memcpy(destinationPath, dest, dest_len);
    destinationPath[dest_len] = '\0';
    // 提取文件名
    const char *src_start = processedArgs;
    const char *src_end = space; // [src_start, src_end)
    const char *last_slash = NULL;
    for (const char *p = src_end - 1; p >= src_start; p--) {
        if (*p == '/') {
            last_slash = p;
            break;
        }
    }
    const char *fname = last_slash ? last_slash + 1 : src_start;
    size_t fname_len = src_end - fname;
    if (fname_len >= FILENAME_LENGTH) {
        fileLocalPath[0] = '\0';
        fileName[0] = '\0';
        return;
    }
    memcpy(fileName, fname, fname_len);
    fileName[fname_len] = '\0';
    // 拼接最终路径
    int written = snprintf(fileLocalPath, FILENAME_MAX, "%s%s", destinationPath, fileName);
    if (written < 0 || (size_t)written >= FILENAME_MAX) {
        fileLocalPath[0] = '\0';
        fileName[0] = '\0';
        return;
    }
}
/**
 * @brief 检查服务器是否有部分文件
 * @param filename 文件名
 * @param fileSize 文件大小
 * @param checkSum 校验和
 */
uint32_t checkServerPartialFile(int socketFd, const char *filename, uint32_t fileSize,
                                const char *checkSum) {
    // 发送检查请求
    packet_t checkPacket;
    memset(&checkPacket, 0, sizeof(checkPacket));
    checkPacket.cmdType = CMD_TYPE_CHECK_PARTIAL;
    // 构造检查数据：filename|fileSize|checksum
    snprintf(checkPacket.buff, sizeof(checkPacket.buff), "%s|%u|%s", filename, fileSize, checkSum);
    checkPacket.len = strlen(checkPacket.buff);

    if (sendRequest(socketFd, &checkPacket) < 0) {
        log_error("Failed to send partial file check request");
        return 0;
    }
    // 接收服务器响应
    char responseBuf[RESPONSE_LENGTH];
    ResponseStatus statusCode;
    DataType dataType;

    int ret = recvResponse(socketFd, responseBuf, sizeof(responseBuf), &statusCode, &dataType);
    if (ret <= 0 || statusCode != STATUS_SUCCESS) {
        return 0; // 没有部分文件，从头开始
    }

    // 解析返回的偏移量
    return (uint32_t)atol(responseBuf);
}
/**
 * @brief 发送文件(优化版)
 * @param socketFd socket描述符
 * @param filepath 文件路径
 * @return 成功返回0,失败返回-1
 */
int putsFile(int socketFd, const char *filepath, uint32_t serverOffset) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file: %s", filepath);
        perror("open");
        return -1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return -1;
    }
    // 计算文件校验和
    char md5_checksum[33];
    if (calculateFileMD5(filepath, md5_checksum) < 0) {
        log_error("Failed to calculate file checksum");
        close(fd);
        return -1;
    }
    // 发送文件大小
    uint32_t fileSize = (uint32_t)st.st_size;
    uint32_t originalFileSize = (uint32_t)st.st_size;
    // 提取文件名
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    // 检查服务器是否存在该文件
    uint32_t ret = sendFileHeader(socketFd, fileSize, serverOffset, filename, md5_checksum);
    if (ret != sizeof(file_transfer_header_t)) {
        log_error("Failed to send transfer header");
        close(fd);
        return -1;
    }
    if (serverOffset > 0) {
        if (lseek(fd, serverOffset, SEEK_SET) != (off_t)serverOffset) {
            log_error("Failed to seek to offset %u", serverOffset);
            close(fd);
            return -1;
        }
        printf("Resuming upload from offset: %u bytes\n", serverOffset);
    }

    // if (sendn(socketFd, &fileSize, sizeof(fileSize)) != sizeof(fileSize)) {
    //     log_error("Failed to send file size");
    //     close(fd);
    //     return -1;
    // }
    // 发送文件内容
    uint32_t totalSent = (off_t)serverOffset;
    char buff[SEND_FILE_BUFF_SIZE];
    while (totalSent < st.st_size) {
        ssize_t bytesRead = read(fd, buff, sizeof(buff));
        if (bytesRead <= 0) {
            if (bytesRead < 0)
                perror("read file");
            break;
        }

        if (sendn(socketFd, buff, bytesRead) != bytesRead) {
            log_error("Failed to send file data");
            break;
        }

        totalSent += bytesRead;
        showProgressBar((uint32_t)totalSent, originalFileSize, "Uploading");
    }
    printf("\n");
    close(fd);
    if (totalSent != st.st_size) {
        log_error("File transfer incomplete: %ld/%ld bytes", totalSent, st.st_size);
        return -1;
    }
    log_info("File uploaded successfully: %s (%ld bytes)", filepath, st.st_size);
    return 0;
}
/**
 * @brief 接收文件(优化版)
 * @param socketFd socket描述符
 * @param filepath 文件路径
 * @param remainingToDownloaded 文件待下载的大小
 * @return 成功返回0,失败返回-1
 */
int getsFile(int socketFd, const char *fileFullPath, uint32_t localOffset) {

    file_transfer_header_t header;
    int ret = recvn(socketFd, &header, sizeof(header));
    if (ret != sizeof(header)) {
        log_error("Failed to receive transfer header");
        return -1;
    }
    // 转换网络字节序
    uint32_t totalFileSize = ntohl(header.fileSize);
    uint32_t serverOffset = ntohl(header.offset);
    log_info("Received file header: totalSize=%u, offset=%u, mode=%d, filename=%s, checkSum=%s",
             totalFileSize, serverOffset, header.mode, header.filename, header.checksum);

    // 验证偏移量一致性
    if (serverOffset != localOffset) {
        log_warn("Offset mismatch: local=%u, server=%u", localOffset, serverOffset);
    }
    // 计算需要接收的数据量
    uint32_t dataToReceive = totalFileSize - serverOffset;

    // 如果没有数据需要接收
    if (dataToReceive == 0) {
        log_info("File already complete");
        return 0;
    }
    // 添加文件大小验证
    if (totalFileSize == 0 || totalFileSize > MAX_FILE_SIZE) {
        log_error("Invalid file size: %u", totalFileSize);
        return -1;
    }

    // 打开文件（根据是否断点续传选择模式）
    int fd;
    if (serverOffset > 0) {
        // 断点续传：追加模式
        fd = open(fileFullPath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            log_error("Failed to open file for resume: %s", fileFullPath);
            perror("open");
            return -1;
        }
        printf("Resuming download from offset: %u bytes\n", serverOffset);
    } else {
        // 新下载：创建/截断
        fd = open(fileFullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            log_error("Failed to create file: %s", fileFullPath);
            perror("open");
            return -1;
        }
    }

    // 接收文件内容
    uint32_t totalReceived = 0;
    char buff[RECEIVE_FILE_BUFF_SIZE];

    printf("Downloading: %s\n", fileFullPath);

    while (totalReceived < dataToReceive) {
        uint32_t remaining = dataToReceive - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        int ret = recvn(socketFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%d, received=%u/%u", ret, totalReceived,
                      dataToReceive);
            goto cleanup_file;
        }
        // 写入文件
        ssize_t written = write(fd, buff, ret);
        if (written != ret) {
            log_error("Failed to write file data: written=%zd, expected=%d", written, ret);
            goto cleanup_file;
        }
        totalReceived += ret;
        // 显示进度条
        showProgressBar(serverOffset + totalReceived, totalFileSize, "Downloading");
    }
    printf("\n"); // 进度条完成后换行
    // 验证文件大小
    if (totalReceived != dataToReceive) {
        log_error("Transfer incomplete: expected=%u, received=%u", dataToReceive, totalReceived);
        goto cleanup_file;
    }

    // 成功完成
    close(fd);
    log_info("File downloaded successfully: %s (%u bytes, offset=%u)", fileFullPath, totalReceived,
             serverOffset);
    return 0;

cleanup_file:
    if (fd >= 0) {
        close(fd);
        // 注意：断点续传时不要删除文件，保留已下载的部分
        if (serverOffset == 0) {
            if (unlink(fileFullPath) < 0) {
                log_warn("Failed to remove incomplete file: %s", fileFullPath);
            }
        }
    }
    return -1;
}
int processResponse(int clientFd, const char *mode) {
    char responseBuf[RESPONSE_LENGTH] = {0};
    ResponseStatus statusCode;
    DataType dataType;

    log_debug("Waiting for %s confirmation...", mode);
    int ret = recvResponse(clientFd, responseBuf, sizeof(responseBuf), &statusCode, &dataType);
    if (ret < 0) {
        log_error("Failed to receive download confirmation");
        return -1;
    }

    if (ret == 0) {
        log_error("Server closed connection");
        return -1;
    }

    // 打印服务器响应
    if (dataType == DATA_TYPE_TEXT) {
        printf("%s", responseBuf);
    }

    log_debug("%s complete", mode);
    return 0;
}
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
 * @brief 处理命令(解析并发送)
 * @param clientFd 客户端socket
 * @param packet 数据包缓冲区
 * @param buf 用户输入缓冲区
 * @return 成功返回0,失败返回-1
 */
int processCommand(int clientFd, packet_t *packet, char *buf) {
    if (!packet || !buf) {
        fprintf(stderr, "Invalid parameters\n");
        return -1;
    }

    char processedArgs[ARGS_LENGTH] = {0};
    char originalInput[1024];
    strncpy(originalInput, buf, sizeof(originalInput) - 1);
    /**
     * 以 puts ./test/test.txt / 为例
     * parseCommand(buf, strlen(buf), packet, processedArgs);
     * packet 解析后结果为:
     * packet->cmdType = CMD_TYPE_PUTS ， 
     * packet->len = strlen("./test.txt /") 
     * packet->buff = "test.txt /"
     * 
     * processedArgs 解析后结果为:
     * processedArgs = "./test/test.txt /"
     * 
     * 以 gets ./test/test.txt / 为例
     * packet 解析后结果为:
     * packet->cmdType = CMD_TYPE_GETS ， 
     * packet->len = strlen("./test.txt /") 
     * packet->buff = "test.txt"
     * processedArgs = "./test/test.txt /"
     */
    // 清空并解析命令
    memset(packet, 0, sizeof(packet_t));
    parseCommand(buf, strlen(buf), packet, processedArgs);
    int ret = 0;
    // 处理 PUTS 命令
    if (packet->cmdType == CMD_TYPE_PUTS) {
        char srcPath[256] = {0};
        char destPath[256] = {0};
        parsePutsArgs(processedArgs, originalInput, srcPath, destPath);
        if (srcPath[0] == '\0') {
            fprintf(stderr, "Invalid puts command format\n");
            return -1;
        }

        //1. 先检查服务器是否有部分文件（在发送 PUTS 命令之前）
        struct stat st;
        if (stat(srcPath, &st) < 0) {
            perror("stat");
            return -1;
        }

        char md5_checksum[33];
        if (calculateFileMD5(srcPath, md5_checksum) < 0) {
            log_error("Failed to calculate file checksum");
            return -1;
        }

        const char *filename = strrchr(srcPath, '/');
        filename = filename ? filename + 1 : srcPath;

        // 检查服务器部分文件
        uint32_t serverOffset =
            checkServerPartialFile(clientFd, filename, (uint32_t)st.st_size, md5_checksum);
        ret = sendRequest(clientFd, packet);
        if (ret < 0) {
            fprintf(stderr, "Failed to send command\n");
            return -1;
        }
        printf("Uploading: %s -> %s\n", srcPath, destPath);
        // 上传文件
        if (putsFile(clientFd, srcPath, serverOffset) < 0) {
            return -1;
        }
        if (processResponse(clientFd, "upload") < 0) {
            return -1;
        }
    } else if (packet->cmdType == CMD_TYPE_GETS) {
        char fileLocalPath[PATH_MAX_LENGTH];
        char fileRemotePath[PATH_MAX_LENGTH];
        char fileName[FILENAME_LENGTH];
        parseGetsArgs(processedArgs, fileRemotePath, fileLocalPath, fileName);
        // 检查本地文件是否存在，获取已下载大小
        uint32_t localFileSize = 0;
        struct stat st;
        if (stat(fileLocalPath, &st) == 0) {
            localFileSize = (uint32_t)st.st_size;
            log_info("Local partial file exists: %s (%u bytes)", fileLocalPath, localFileSize);
        }
        snprintf(packet->buff, sizeof(packet->buff), "%s|%u", fileRemotePath, localFileSize);
        packet->len = strlen(packet->buff);

        // 发送 GETS 请求
        int ret = sendRequest(clientFd, packet);
        if (ret < 0) {
            fprintf(stderr, "Failed to send command\n");
            return -1;
        }
        if (getsFile(clientFd, fileLocalPath, localFileSize) < 0) {
            return -1;
        }
        if (processResponse(clientFd, "download") < 0) {
            return -1;
        }
    } else {
        // 其他命令正常发送
        ret = sendRequest(clientFd, packet);
        if (ret < 0) {
            fprintf(stderr, "Failed to send command\n");
            return -1;
        }
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
    ResponseStatus statusCode;
    DataType dataType;
    if (!buf || bufLen <= 0) {
        return -1;
    }
    // 接收响应
    int ret = recvResponse(clientFd, buf, bufLen, &statusCode, &dataType);
    if (ret <= 0) {
        if (ret == 0) {
            printf("server closed connection\n");
        } else {
            perror("recv");
        }
        return ret;
    }
    // 处理状态码
    if (statusCode != STATUS_SUCCESS) {
        fprintf(stderr, "[Server Error] ");
        switch (statusCode) {
            case STATUS_FAIL:
                fprintf(stderr, "Operation failed\n");
                break;
            case STATUS_NOT_FOUND:
                fprintf(stderr, "Resource not found\n");
                break;
            case STATUS_PERMISSION_DENIED:
                fprintf(stderr, "Permission denied\n");
                break;
            case STATUS_INVALID_PARAM:
                fprintf(stderr, "Invalid parameter\n");
                break;
            default:
                fprintf(stderr, "Unknown error (code: %d)\n", statusCode);
                break;
        }
    }
    // 根据数据类型处理数据
    if (dataType == DATA_TYPE_TEXT) {
        printf("%s", buf);
    } else if (dataType == DATA_TYPE_BINARY) {
        // 二进制数据处理(例如保存文件)
        printf("[Binary data received: %d bytes]\n", ret);
        // TODO: 保存到文件
        // saveToFile(buf, ret);
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