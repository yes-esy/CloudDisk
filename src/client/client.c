/**
 * @FilePath     : /CloudDisk/src/client/client.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-23 23:59:14
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
#include <sys/mman.h>

extern char workforlder[PATH_MAX_LENGTH];
/**
 * @brief 传统方式发送文件
 */
static int putsFileTraditional(int socketFd, const char *filepath, uint32_t fileSize,
                               uint32_t serverOffset) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file: %s", filepath);
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

    uint32_t totalSent = serverOffset;
    char buff[SEND_FILE_BUFF_SIZE];

    while (totalSent < fileSize) {
        ssize_t bytesRead = read(fd, buff, sizeof(buff));
        if (bytesRead <= 0) {
            if (bytesRead < 0)
                perror("read file");
            break;
        }

        if (sendn(socketFd, buff, bytesRead) != bytesRead) {
            log_error("Failed to send file data");
            close(fd);
            return -1;
        }

        totalSent += bytesRead;
        showProgressBar(totalSent, fileSize, "Uploading");
    }

    printf("\n");
    close(fd);

    if (totalSent != fileSize) {
        log_error("File transfer incomplete: %u/%u bytes", totalSent, fileSize);
        return -1;
    }

    log_info("File uploaded successfully: %s (%u bytes)", filepath, fileSize);
    return 0;
}
/**
 * @brief 使用 mmap 发送大文件
 */
static int putsFileWithMmap(int socketFd, const char *filepath, uint32_t fileSize,
                            uint32_t serverOffset) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file: %s", filepath);
        return -1;
    }

    // 映射整个文件
    void *mapped = mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    // 建议内核顺序读取
    madvise(mapped, fileSize, MADV_SEQUENTIAL);

    uint32_t dataToSend = fileSize - serverOffset;
    uint32_t totalSent = 0;
    const char *readPtr = (const char *)mapped + serverOffset;

    while (totalSent < dataToSend) {
        uint32_t remaining = dataToSend - totalSent;
        size_t toSend = (remaining < SEND_FILE_BUFF_SIZE) ? remaining : SEND_FILE_BUFF_SIZE;

        ssize_t ret = sendn(socketFd, readPtr, toSend);
        if (ret != (ssize_t)toSend) {
            log_error("Failed to send file data: ret=%zd, expected=%zu", ret, toSend);
            munmap(mapped, fileSize);
            close(fd);
            return -1;
        }

        readPtr += ret;
        totalSent += ret;
        showProgressBar(serverOffset + totalSent, fileSize, "Uploading");
    }

    printf("\n");
    munmap(mapped, fileSize);
    close(fd);

    log_info("File uploaded successfully (mmap): %s (%u bytes)", filepath, fileSize);
    return 0;
}
/**
 * @brief 传统方式接收文件
 */
static int getsFileTraditional(int socketFd, const char *fileFullPath, uint32_t totalFileSize,
                               uint32_t serverOffset, uint32_t dataToReceive) {
    int fd;
    if (serverOffset > 0) {
        fd = open(fileFullPath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            log_error("Failed to open file for resume: %s", fileFullPath);
            return -1;
        }
        printf("Resuming download from offset: %u bytes\n", serverOffset);
    } else {
        fd = open(fileFullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            log_error("Failed to create file: %s", fileFullPath);
            return -1;
        }
    }

    uint32_t totalReceived = 0;
    char buff[RECEIVE_FILE_BUFF_SIZE];

    while (totalReceived < dataToReceive) {
        uint32_t remaining = dataToReceive - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        int ret = recvn(socketFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%d", ret);
            goto cleanup_file;
        }

        ssize_t written = write(fd, buff, ret);
        if (written != ret) {
            log_error("Failed to write file data");
            goto cleanup_file;
        }

        totalReceived += ret;
        showProgressBar(serverOffset + totalReceived, totalFileSize, "Downloading");
    }

    printf("\n");
    close(fd);

    log_info("File downloaded successfully: %s (%u bytes)", fileFullPath, totalReceived);
    return 0;

cleanup_file:
    close(fd);
    if (serverOffset == 0) {
        unlink(fileFullPath);
    }
    return -1;
}
/**
 * @brief 使用 mmap 接收大文件
 */
static int getsFileWithMmap(int socketFd, const char *fileFullPath, uint32_t totalFileSize,
                            uint32_t serverOffset, uint32_t dataToReceive) {
    int fd = -1;
    void *mapped = MAP_FAILED;
    int result = -1;

    // 打开/创建文件
    if (serverOffset > 0) {
        fd = open(fileFullPath, O_RDWR);
    } else {
        fd = open(fileFullPath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }

    if (fd < 0) {
        log_error("Failed to open file: %s", fileFullPath);
        return -1;
    }

    // 扩展文件到目标大小
    if (ftruncate(fd, totalFileSize) < 0) {
        log_error("Failed to extend file: %s", strerror(errno));
        goto cleanup;
    }

    // 映射需要写入的部分
    mapped = mmap(NULL, dataToReceive, PROT_WRITE, MAP_SHARED, fd, serverOffset);
    if (mapped == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        goto cleanup;
    }

    // 直接接收到映射内存
    uint32_t totalReceived = 0;
    char *writePtr = (char *)mapped;

    while (totalReceived < dataToReceive) {
        uint32_t remaining = dataToReceive - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        ssize_t ret = recvn(socketFd, writePtr, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%zd", ret);
            goto cleanup;
        }

        writePtr += ret;
        totalReceived += ret;
        showProgressBar(serverOffset + totalReceived, totalFileSize, "Downloading");
    }

    printf("\n");

    // 同步到磁盘
    if (msync(mapped, dataToReceive, MS_SYNC) < 0) {
        log_warn("msync failed: %s", strerror(errno));
    }

    log_info("File downloaded successfully (mmap): %s (%u bytes)", fileFullPath, totalReceived);
    result = 0;

cleanup:
    if (mapped != MAP_FAILED) {
        munmap(mapped, dataToReceive);
    }
    if (fd >= 0) {
        close(fd);
    }
    return result;
}
/**
 * @brief 发送用户名并接收验证结果
 * @param sockfd socket描述符
 * @param packet_p 输出参数:服务器响应包
 * @return 成功返回0
 */
int sendLoginUsername(int sockfd, packet_t *packet_p) {
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
int sendLoginPassword(int sockfd, packet_t *packet_p) {
    packet_t packet;
    int ret;
    char responseBuf[RESPONSE_LENGTH] = {0};
    ResponseStatus statusCode;
    DataType dataType;

    while (1) {
        char *password = getpass(PASSWORD);

        unsigned char salt_bin[16]; // SALT_LEN = 16
        if (hex_to_bin(packet_p->buff, salt_bin, sizeof(salt_bin)) != 0) {
            log_error("Failed to parse salt from server");
            continue;
        }

        char hash_hex[KEY_LEN * 2 + 1];
        if (derive_password_with_salt(password, salt_bin, sizeof(salt_bin), hash_hex,
                                      sizeof(hash_hex))
            != 0) {
            log_error("Failed to derive password hash");
            continue;
        }

        // 发送密文
        packet.len = strlen(hash_hex);
        packet.cmdType = TASK_LOGIN_VERIFY_PASSWORD;
        strncpy(packet.buff, hash_hex, sizeof(packet.buff) - 1);

        ret = sendRequest(sockfd, &packet);
        if (ret <= 0) {
            printf("unknown error,try again\n");
            log_error("Failed to send password");
            continue;
        }

        log_info("send password hash: %s", hash_hex);
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

        printf("%s", responseBuf);

        // 接收当前工作目录
        memset(workforlder, 0, sizeof(workforlder));
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
 * @brief 用户注册
 * @param sockfd 客户端fd
 */
void userRegister(int sockfd) {
    char usernameInput[USERNAME_LENGTH];
    char passwordInput[PASSWORD_LENGTH];
    char againPasswordInput[PASSWORD_LENGTH];
    int passwordValid = 0;
    int usernameValid = 0;
    packet_t usernamePacket;
    packet_t passwordPacket;

    while (!usernameValid) {
        printf("please input a valid username(length >= 3):\n");
        int ret = read(STDIN_FILENO, usernameInput, sizeof(usernameInput));
        if (ret <= 1)
            continue;
        usernameInput[ret - 1] = '\0';

        memset(&usernamePacket, 0, sizeof(usernamePacket));
        strncpy(usernamePacket.buff, usernameInput, sizeof(usernamePacket.buff) - 1);
        usernamePacket.len = (uint32_t)strlen(usernamePacket.buff);
        usernamePacket.cmdType = TASK_REGISTER_USERNAME;

        if (sendRequest(sockfd, &usernamePacket) <= 0) {
            printf("send error, try again\n");
            continue;
        }

        char response[RESPONSE_LENGTH] = {0};
        ResponseStatus status;
        DataType dataType;
        int r = recvResponse(sockfd, response, sizeof(response), &status, &dataType);
        if (r <= 0) {
            printf("recv error or server closed\n");
            continue;
        }
        printf("%s", response);
        if (status == STATUS_SUCCESS) {
            usernameValid = 1;
            break;
        }
    }
    while (!passwordValid) {
        printf("please input a valid password(length >= 8):\n");
        int ret = read(STDIN_FILENO, passwordInput, sizeof(passwordInput));
        if (ret <= 1)
            continue;
        passwordInput[ret - 1] = '\0';
        if ((int)strlen(passwordInput) < 8)
            continue;

        printf("please input this password again:\n");
        ret = read(STDIN_FILENO, againPasswordInput, sizeof(againPasswordInput));
        if (ret <= 1)
            continue;
        againPasswordInput[ret - 1] = '\0';
        if (strcmp(passwordInput, againPasswordInput) != 0) {
            printf("The passwords entered twice were not the same.\n");
            continue;
        }

        memset(&passwordPacket, 0, sizeof(passwordPacket));
        passwordPacket.cmdType = TASK_REGISTER_PASSWORD;
        snprintf(passwordPacket.buff, sizeof(passwordPacket.buff), "%s\n%s", usernameInput,
                 passwordInput);
        passwordPacket.len = (uint32_t)strlen(passwordPacket.buff);

        if (sendRequest(sockfd, &passwordPacket) <= 0) {
            printf("send error, try again\n");
            continue;
        }

        char response[RESPONSE_LENGTH] = {0};
        ResponseStatus status;
        DataType dataType;
        int r = recvResponse(sockfd, response, sizeof(response), &status, &dataType);
        if (r <= 0) {
            printf("recv error or server closed\n");
            continue;
        }
        printf("%s", response);
        if (status == STATUS_SUCCESS) {
            passwordValid = 1;
            break;
        }
    }
}
/**
 * @brief : 用户登录 
 * @param sockfd: 客户端socket
 * @return void
**/
void userLogin(int sockfd) {
    int ret;
    packet_t packet;
    sendLoginUsername(sockfd, &packet);
    sendLoginPassword(sockfd, &packet);
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
    close(fd); // 先关闭，后面根据方式重新打开
    // 计算文件校验和
    char md5_checksum[33];
    uint32_t fileSize = (uint32_t)st.st_size;
    if (calculateFileMD5(filepath, md5_checksum) < 0) {
        log_error("Failed to calculate file checksum");
        return -1;
    }
    // 发送文件大小
    uint32_t originalFileSize = (uint32_t)st.st_size;
    // 提取文件名
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    // 发送文件头
    uint32_t ret = sendFileHeader(socketFd, fileSize, serverOffset, filename, md5_checksum);
    if (ret != sizeof(file_transfer_header_t)) {
        log_error("Failed to send transfer header");
        close(fd);
        return -1;
    }
    // 根据文件大小选择传输方式
    uint32_t dataToSend = fileSize - serverOffset;
    if (dataToSend == 0) {
        log_info("File already uploaded completely");
        return 0;
    }

    if (dataToSend >= MMAP_THRESHOLD) {
        log_info("Using mmap for large file upload (%u bytes)", dataToSend);
        return putsFileWithMmap(socketFd, filepath, fileSize, serverOffset);
    } else {
        log_info("Using traditional method for file upload (%u bytes)", dataToSend);
        return putsFileTraditional(socketFd, filepath, fileSize, serverOffset);
    }
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
    // 接收文件头
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

    // 根据文件大小选择传输方式
    if (dataToReceive >= MMAP_THRESHOLD) {
        log_info("Using mmap for large file download (%u bytes)", dataToReceive);
        return getsFileWithMmap(socketFd, fileFullPath, totalFileSize, serverOffset, dataToReceive);
    } else {
        log_info("Using traditional method for file download (%u bytes)", dataToReceive);
        return getsFileTraditional(socketFd, fileFullPath, totalFileSize, serverOffset,
                                   dataToReceive);
    }
}
/**
 * @brief 处理 PUTS 命令
 */
static int handlePutsCommand(int clientFd, packet_t *packet, const char *processedArgs,
                             const char *originalInput) {
    char srcPath[256] = {0};
    char destPath[256] = {0};
    parsePutsArgs(processedArgs, originalInput, srcPath, destPath);

    if (srcPath[0] == '\0') {
        fprintf(stderr, "Invalid puts command format\n");
        return -1;
    }

    // 获取本地文件信息
    LocalFileInfo fileInfo;
    if (getLocalFileInfo(srcPath, &fileInfo) < 0) {
        return -1;
    }

    // 检查服务器部分文件
    uint32_t serverOffset = checkServerPartialFile(clientFd, fileInfo.filename, fileInfo.fileSize,
                                                   fileInfo.md5Checksum);

    // 发送命令
    if (sendRequest(clientFd, packet) < 0) {
        fprintf(stderr, "Failed to send command\n");
        return -1;
    }

    printf("Uploading: %s -> %s\n", srcPath, destPath);

    // 上传文件
    if (putsFile(clientFd, srcPath, serverOffset) < 0) {
        return -1;
    }

    return processResponse(clientFd, "upload");
}

/**
 * @brief 处理 GETS 命令
 */
static int handleGetsCommand(int clientFd, packet_t *packet, const char *processedArgs) {
    char fileLocalPath[PATH_MAX_LENGTH];
    char fileRemotePath[PATH_MAX_LENGTH];
    char fileName[FILENAME_LENGTH];

    parseGetsArgs((char *)processedArgs, fileRemotePath, fileLocalPath, fileName);

    // 检查本地文件
    uint32_t localFileSize = getLocalPartialFileSize(fileLocalPath);
    if (localFileSize > 0) {
        log_info("Local partial file exists: %s (%u bytes)", fileLocalPath, localFileSize);
    }

    // 构造请求
    snprintf(packet->buff, sizeof(packet->buff), "%s|%u", fileRemotePath, localFileSize);
    packet->len = strlen(packet->buff);

    // 发送请求
    if (sendRequest(clientFd, packet) < 0) {
        fprintf(stderr, "Failed to send command\n");
        return -1;
    }

    // 下载文件
    if (getsFile(clientFd, fileLocalPath, localFileSize) < 0) {
        return -1;
    }

    return processResponse(clientFd, "download");
}

/**
 * @brief 处理普通命令
 */
static int handleNormalCommand(int clientFd, packet_t *packet) {
    if (sendRequest(clientFd, packet) < 0) {
        fprintf(stderr, "Failed to send command\n");
        return -1;
    }
    return 0;
}
/**
 * @brief 处理服务器的Response
 * @param clienFd sockfd
 * @param mode "upload" or download
 */
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

    memset(packet, 0, sizeof(packet_t));
    parseCommand(buf, strlen(buf), packet, processedArgs);

    switch (packet->cmdType) {
        case CMD_TYPE_PUTS:
            return handlePutsCommand(clientFd, packet, processedArgs, originalInput);

        case CMD_TYPE_GETS:
            return handleGetsCommand(clientFd, packet, processedArgs);

        default:
            return handleNormalCommand(clientFd, packet);
    }
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