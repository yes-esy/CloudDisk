/**
 * @FilePath     : /CloudDisk/src/client/client.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-20 16:56:23
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "client.h"
#include <unistd.h>
#include "net.h"
#include "protocol.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "log.h"
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
 * @param sockfd:
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
 * @brief 从处理后的参数中提取源文件路径和目标路径
 * @param processedArgs 格式: "path/filename destpath"
 * @param fileName 输出:原始源文件名(从原始输入恢复)
 * @param fileDestSrc 输出:目标路径
 * @param originalInput 原始用户输入
 */
void parseGetsArgs(char *processedArgs, char *fileFullPath) {
    const char *space = strchr(processedArgs, ' ');
    if (!space || !fileFullPath) {
        if (fileFullPath)
            fileFullPath[0] = '\0';
        return;
    }
    size_t len = space - processedArgs;
    if (len >= 256)
        len = 255; // 防止溢出
    strncpy(fileFullPath, processedArgs, len);
    fileFullPath[len] = '\0';
}

/**
 * @brief 发送文件(优化版)
 * @param socketFd socket描述符
 * @param filepath 文件路径
 * @return 成功返回0,失败返回-1
 */
int putsFile(int socketFd, const char *filepath) {
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

    // 发送文件大小
    uint32_t fileSize = htonl((uint32_t)st.st_size);
    uint32_t originalFileSize = (uint32_t)st.st_size;
    if (sendn(socketFd, &fileSize, sizeof(fileSize)) != sizeof(fileSize)) {
        log_error("Failed to send file size");
        close(fd);
        return -1;
    }

    // 发送文件内容
    off_t totalSent = 0;
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
 * @return 成功返回0,失败返回-1
 */
int getsFile(int socketFd, const char *fileFullPath) {
    uint32_t fileSizeNet = 0;
    if (recvn(socketFd, (void *)&fileSizeNet, sizeof(fileSizeNet)) != sizeof(fileSizeNet)) {
        log_error("recv file size error");
        return -1;
    }

    uint32_t fileSize = (uint32_t)ntohl(fileSizeNet);
    log_info("received file size is %u", fileSize);

    // 添加文件大小验证
    if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
        log_error("Invalid file size: %u", fileSize);
        return -1;
    }

    // 创建文件
    int fd = open(fileFullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        log_error("Failed to create file: %s", fileFullPath);
        perror("open");
        return -1;
    }

    // 接收文件内容
    uint32_t totalReceived = 0;
    char buff[RECEIVE_FILE_BUFF_SIZE];

    printf("Downloading: %s\n", fileFullPath);

    while (totalReceived < fileSize) {
        uint32_t remaining = fileSize - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        int ret = recvn(socketFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%d, received=%u/%u", ret, totalReceived,
                      fileSize);
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
        showProgressBar(totalReceived, fileSize, "Downloading");

        log_debug("Received %u/%u bytes", totalReceived, fileSize);
    }

    printf("\n"); // 进度条完成后换行

    // 验证文件大小
    if (totalReceived != fileSize) {
        log_error("File size mismatch: expected=%u, received=%u", fileSize, totalReceived);
        goto cleanup_file;
    }

    // 成功完成
    close(fd);
    log_info("File downloaded successfully: %s (%u bytes)", fileFullPath, fileSize);
    return 0;

cleanup_file:
    if (fd >= 0) {
        close(fd);
        // 删除不完整的文件
        if (unlink(fileFullPath) < 0) {
            log_warn("Failed to remove incomplete file: %s", fileFullPath);
        }
    }
    return -1;
}
/**
 * @brief 发送请求给服务器
 * @param clientFd 客户端socket
 * @param packet 数据包
 * @return 成功返回发送字节数,失败返回-1
 */
int sendRequest(int clientFd, const packet_t *packet) {
    if (!packet) {
        return -1;
    }
    packet_t netPacket;
    netPacket.len = htonl(packet->len);
    netPacket.cmdType = htonl(packet->cmdType);
    memcpy(netPacket.buff, packet->buff, packet->len);
    size_t totalSize = sizeof(netPacket.len) + sizeof(netPacket.cmdType) + packet->len;
    int ret = sendn(clientFd, &netPacket, totalSize);
    if (ret != (int)totalSize) {
        perror("sendRequest failed");
        return -1;
    }
    return ret;
}
/**
 * @brief 接收服务器响应(只负责接收,不处理数据)
 * @param clientFd 客户端socket
 * @param buf 接收缓冲区
 * @param bufLen 缓冲区大小
 * @param statusCode 输出参数:状态码
 * @param dataType 输出参数:数据类型
 * @return 成功返回接收的数据长度,失败返回-1,连接关闭返回0
 */
int recvResponse(int clientFd, char *buf, int bufLen, ResponseStatus *statusCode,
                 DataType *dataType) {
    if (!buf || bufLen <= 0 || !statusCode || !dataType) {
        return -1;
    }
    // 1. 接收响应头
    response_header_t header;
    int ret = recvn(clientFd, &header, sizeof(header));
    if (ret != sizeof(header)) {
        if (ret == 0) {
            return 0; // 连接关闭
        }
        perror("recv response header");
        return -1;
    }
    // 2. 转换网络字节序并恢复枚举类型
    uint32_t dataLen = ntohl(header.dataLen);
    *statusCode = (ResponseStatus)ntohl((uint32_t)header.statusCode);
    *dataType = (DataType)ntohl((uint32_t)header.dataType);
    // 3. 检查数据长度
    if (dataLen == 0) {
        return 0;
    }
    if (dataLen >= (uint32_t)bufLen) {
        fprintf(stderr, "Response too large: %u bytes (buffer: %d)\n", dataLen, bufLen);
        return -1;
    }
    // 4. 接收响应数据
    ret = recvn(clientFd, buf, dataLen);
    if (ret != (int)dataLen) {
        perror("recv response data");
        return -1;
    }
    buf[ret] = '\0';
    return ret;
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

    // 发送请求
    int ret = sendRequest(clientFd, packet);
    if (ret < 0) {
        fprintf(stderr, "Failed to send command\n");
        return -1;
    }
    // 处理 PUTS 命令
    if (packet->cmdType == CMD_TYPE_PUTS) {
        char srcPath[256] = {0};
        char destPath[256] = {0};
        parsePutsArgs(processedArgs, originalInput, srcPath, destPath);
        if (srcPath[0] == '\0') {
            fprintf(stderr, "Invalid puts command format\n");
            return -1;
        }

        printf("Uploading: %s -> %s\n", srcPath, destPath);
        // 上传文件
        if (putsFile(clientFd, srcPath) < 0) {
            return -1;
        }

        char responseBuf[RESPONSE_LENGTH] = {0};
        ResponseStatus statusCode;
        DataType dataType;

        log_debug("Waiting for upload confirmation...");
        int ret = recvResponse(clientFd, responseBuf, sizeof(responseBuf), &statusCode, &dataType);
        if (ret < 0) {
            log_error("Failed to receive upload confirmation");
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

        log_debug("Upload complete");
    } else if (packet->cmdType == CMD_TYPE_GETS) {
        char fileFullPath[256];
        parseGetsArgs(processedArgs, fileFullPath);
        if (getsFile(clientFd, fileFullPath) < 0) {
            return -1;
        }
        char responseBuf[RESPONSE_LENGTH] = {0};
        ResponseStatus statusCode;
        DataType dataType;

        log_debug("Waiting for download confirmation...");
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

        log_debug("download complete");
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