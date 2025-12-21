/**
 * @FilePath     : /CloudDisk/src/common/command.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-21 21:27:03
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "command.h"
#include "net.h"
#include "threadpool.h"
#include "path.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "log.h"
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include "list.h"
#include "login.h"
#include "tools.h"
extern ListNode *userList;
static char currentVirtualPath[PATH_MAX_LENGTH] = "/";
/**
 * @brief 发送响应给客户端
 */
ssize_t sendResponse(int peerFd, ResponseStatus statusCode, DataType dataType, const char *data,
                     size_t dataLen) {
    response_header_t header;

    // 填充响应头并转换为网络字节序
    header.dataLen = htonl((uint32_t)dataLen);
    header.statusCode = (ResponseStatus)htonl((uint32_t)statusCode);
    header.dataType = (DataType)htonl((uint32_t)dataType);
    log_debug("Sending response: status=%d, dataType=%d, dataLen=%zu", statusCode, dataType,
              dataLen);
    // 1. 发送响应头
    ssize_t ret = sendn(peerFd, &header, sizeof(header));
    if (ret != sizeof(header)) {
        log_error("Failed to send response header: %s", strerror(errno));
        return -1;
    }
    // 2. 发送响应数据
    if (dataLen > 0 && data != NULL) {
        ret = sendn(peerFd, data, dataLen);
        if (ret != (ssize_t)dataLen) {
            log_error("Failed to send response data: expected=%zu, sent=%zd", dataLen, ret);
            return -1;
        }
    }
    log_info("Response sent successfully: %zu bytes", sizeof(header) + dataLen);
    return sizeof(header) + dataLen;
}
/**
 * @brief 检查是否存在某一文件
 * @param task 执行该检查的线程
 */
void checkPartialFile(task_t *task) {
    log_info("client request check partial file,receive data: %s", task->data);
    char filename[256], checksum[33];
    uint32_t expectedSize;
    char response[RESPONSE_LENGTH] = {0};
    ResponseStatus statusCode = STATUS_SUCCESS;

    // 解析请求：filename|fileSize|checksum
    if (sscanf(task->data, "%255[^|]|%u|%32s", filename, &expectedSize, checksum) != 3) {
        statusCode = STATUS_INVALID_PARAM;
        strcpy(response, "0");
        goto send_response;
    }

    // 构造部分文件路径
    char partialPath[PATH_MAX_LENGTH];
    char currentRealPath[PATH_MAX_LENGTH];

    if (virtualPathToReal(currentVirtualPath, currentRealPath, sizeof(currentRealPath)) < 0) {
        statusCode = STATUS_FAIL;
        strcpy(response, "0");
        goto send_response;
    }

    snprintf(partialPath, sizeof(partialPath), "%s/%s", currentRealPath, filename);

    // 检查部分文件
    struct stat st;
    if (stat(partialPath, &st) == 0 && S_ISREG(st.st_mode)) {
        uint32_t partialSize = (uint32_t)st.st_size;

        // 验证部分文件有效性（大小合理）
        if (partialSize < expectedSize) {
            snprintf(response, sizeof(response), "%u", partialSize);
        } else {
            // 部分文件大小异常，删除重传
            unlink(partialPath);
            strcpy(response, "0");
        }
    } else {
        strcpy(response, "0");
    }
send_response:
    log_info("check partial file response:%s", response);
    sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, strlen(response));
}
/**
 * @brief        : 当前工作目录命令
 * @param         {task_t} *task: 任务结构
 * @return        {*}
**/
void pwdCommand(task_t *task) {
    log_info("Executing pwd command (fd=%d)", task->peerFd);
    ResponseStatus statusCode = STATUS_SUCCESS;
    char response[RESPONSE_LENGTH] = {0};
    size_t responseLen = 0;

    // 直接返回当前虚拟路径，无需进行路径转换
    log_debug("Current virtual path: %s", currentVirtualPath);
    responseLen = snprintf(response, sizeof(response), "%s\n", currentVirtualPath);

    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("pwd: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("pwd command completed: status=%d, path=%s", statusCode, currentVirtualPath);
    }
}
/**
 * @brief 列出目录内容命令
 */
void lsCommand(task_t *task) {
    log_info("Executing ls command (fd=%d)", task->peerFd);
    ResponseStatus statusCode = STATUS_SUCCESS;
    char realPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_LENGTH] = {0};
    size_t offset = 0;
    int count = 0;

    // 转换虚拟路径到真实路径
    if (virtualPathToReal(currentVirtualPath, realPath, sizeof(realPath)) < 0) {
        log_error("Virtual path conversion failed: %s", currentVirtualPath);
        statusCode = STATUS_FAIL;
        offset = snprintf(response, sizeof(response), "ls error: path conversion failed\n");
        goto send_response;
    }

    log_debug("Listing directory: %s", realPath);

    // 打开目录
    DIR *dir = opendir(realPath);
    if (dir == NULL) {
        log_error("opendir failed: %s: %s", realPath, strerror(errno));
        statusCode = STATUS_FAIL;
        offset = snprintf(response, sizeof(response), "ls error: %s\n", strerror(errno));
        goto send_response;
    }

    // 添加表头
    int written = snprintf(response + offset, sizeof(response) - offset, "%-6s  %12s  %-16s  %s\n",
                           "Type", "Size", "Modified", "Name");
    if (written > 0 && offset + written < sizeof(response)) {
        offset += written;
        written = snprintf(response + offset, sizeof(response) - offset, "%-6s  %12s  %-16s  %s\n",
                           "------", "------------", "----------------", "--------------------");
        if (written > 0 && offset + written < sizeof(response)) {
            offset += written;
        }
    }

    // 遍历目录项
    struct dirent *entry;
    struct stat statbuf;

    while ((entry = readdir(dir)) != NULL) {
        char fullPath[PATH_MAX_LENGTH];
        getFileFullPath(entry->d_name, realPath, fullPath);

        if (fullPath[0] == '\0') {
            log_warn("Path too long, skipping: %s", entry->d_name);
            continue;
        }

        if (stat(fullPath, &statbuf) != 0) {
            log_warn("stat failed for '%s': %s", entry->d_name, strerror(errno));
            continue;
        }

        // 确定文件类型
        const char *typeStr;
        char fileName[PATH_MAX_LENGTH];

        if (S_ISDIR(statbuf.st_mode)) {
            typeStr = "<DIR>";
            snprintf(fileName, sizeof(fileName), "%s/", entry->d_name);
        } else if (S_ISREG(statbuf.st_mode)) {
            typeStr = "FILE";
            strncpy(fileName, entry->d_name, sizeof(fileName) - 1);
        } else if (S_ISLNK(statbuf.st_mode)) {
            typeStr = "LINK";
            strncpy(fileName, entry->d_name, sizeof(fileName) - 1);
        } else {
            typeStr = "OTHER";
            strncpy(fileName, entry->d_name, sizeof(fileName) - 1);
        }

        // 格式化文件大小
        char sizeStr[13];
        if (S_ISDIR(statbuf.st_mode)) {
            snprintf(sizeStr, sizeof(sizeStr), "-");
        } else if (statbuf.st_size < 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%ld B", statbuf.st_size);
        } else if (statbuf.st_size < 1024 * 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", statbuf.st_size / 1024.0);
        } else if (statbuf.st_size < 1024 * 1024 * 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", statbuf.st_size / (1024.0 * 1024.0));
        } else {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f GB",
                     statbuf.st_size / (1024.0 * 1024.0 * 1024.0));
        }

        // 格式化修改时间
        char timeStr[17];
        struct tm *tm_info = localtime(&statbuf.st_mtime);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm_info);

        // 添加到响应缓冲区
        written = snprintf(response + offset, sizeof(response) - offset, "%-6s  %12s  %-16s  %s\n",
                           typeStr, sizeStr, timeStr, fileName);

        if (written > 0 && offset + written < sizeof(response)) {
            offset += written;
            count++;
        } else {
            log_warn("Response buffer full, truncating at %d items", count);
            break;
        }
    }

    closedir(dir);

    // 添加汇总信息
    if (count == 0) {
        offset = snprintf(response, sizeof(response), "Empty directory\n");
        log_debug("Directory is empty");
    } else {
        if (offset + 30 < sizeof(response)) {
            written = snprintf(response + offset, sizeof(response) - offset,
                               "\nTotal: %d item(s)\n", count);
            if (written > 0) {
                offset += written;
            }
        }
        log_debug("Listed %d items from directory", count);
    }

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, offset) < 0) {
        log_error("ls: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("ls command completed: status=%d, items=%d, bytes=%zu", statusCode, count, offset);
    }
}
/**
 * @brief 进入某一目录的命令
 */
void cdCommand(task_t *task) {
    log_info("Executing cd command (fd=%d)", task->peerFd);
    char currentRealPath[PATH_MAX_LENGTH];
    char response[RESPONSE_LENGTH];
    char directoryFullPath[PATH_MAX_LENGTH];
    char resolvedPath[PATH_MAX_LENGTH];
    char newVirtualPath[PATH_MAX_LENGTH];
    ResponseStatus statusCode = STATUS_SUCCESS;
    ssize_t responseLen = 0;

    // 1. 检查参数是否为空
    if (task->data[0] == '\0') {
        log_error("cd: Empty directory name");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "cd error: directory name cannot be empty\n");
        goto send_response;
    }

    log_debug("Changing to directory: %s", task->data);

    // 2. 获取当前真实路径
    if (virtualPathToReal(currentVirtualPath, currentRealPath, sizeof(currentRealPath)) < 0) {
        log_error("Path conversion failed: %s", currentVirtualPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "cd error: path conversion failed\n");
        goto send_response;
    }

    log_debug("Current real path: %s", currentRealPath);

    // 3. 获取目标目录的完整路径 (参数顺序: fileName, path, fullPath)
    if (getFileFullPath(task->data, currentRealPath, directoryFullPath) < 0) {
        log_error("get full path failed: current path %s, next path %s", currentRealPath,
                  task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "cd error: get full path failed.\n");
        goto send_response;
    }

    log_debug("Target directory full path: %s", directoryFullPath);

    // 4. 使用 realpath 规范化路径（处理 .. 和 . 等相对路径）
    if (realpath(directoryFullPath, resolvedPath) == NULL) {
        log_error("realpath failed: %s: %s", directoryFullPath, strerror(errno));
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "cd error: '%s' does not exist or permission denied\n", task->data);
        goto send_response;
    }

    log_debug("Resolved path: %s", resolvedPath);

    // 5. 检查解析后的路径是否在云盘根目录下
    if (strncmp(resolvedPath, CLOUD_DISK_ROOT, strlen(CLOUD_DISK_ROOT)) != 0) {
        log_error("Path outside cloud disk root: %s", resolvedPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "cd error: cannot access path outside cloud disk\n");
        goto send_response;
    }

    // 6. 检查是否为目录
    struct stat st;
    if (stat(resolvedPath, &st) != 0) {
        log_error("stat failed for: %s", resolvedPath);
        statusCode = STATUS_FAIL;
        responseLen =
            snprintf(response, sizeof(response), "cd error: cannot access '%s'\n", task->data);
        goto send_response;
    }

    if (!S_ISDIR(st.st_mode)) {
        log_error("Target is not a directory: %s", resolvedPath);
        statusCode = STATUS_FAIL;
        responseLen =
            snprintf(response, sizeof(response), "cd error: '%s' is not a directory\n", task->data);
        goto send_response;
    }

    // 7. 检查执行权限（目录需要执行权限才能进入）
    // if (access(resolvedPath, X_OK) != 0) {
    //     log_error("No execute permission for directory: %s", resolvedPath);
    //     statusCode = STATUS_FAIL;
    //     responseLen = snprintf(response, sizeof(response), "cd error: permission denied\n");
    //     goto send_response;
    // }

    // 8. 将真实路径转换为虚拟路径
    if (realPathToVirtual(resolvedPath, newVirtualPath, sizeof(newVirtualPath)) != 0) {
        log_error("Failed to convert real path to virtual: %s", resolvedPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "cd error: path conversion failed\n");
        goto send_response;
    }

    // 9. 更新当前虚拟路径
    strncpy(currentVirtualPath, newVirtualPath, sizeof(currentVirtualPath) - 1);
    currentVirtualPath[sizeof(currentVirtualPath) - 1] = '\0';

    log_info("Changed directory to: %s (virtual: %s)", resolvedPath, currentVirtualPath);
    responseLen =
        snprintf(response, sizeof(response), "Changed to directory: %s\n", currentVirtualPath);

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("cd: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("cd command completed: status=%d, new_path=%s", statusCode,
                 statusCode == STATUS_SUCCESS ? currentVirtualPath : "error");
    }
}
/**
 * @brief 创建目录命令
 * @param task 任务结构
 */
void mkdirCommand(task_t *task) {
    log_info("Executing mkdir command (fd=%d)", task->peerFd);
    ResponseStatus statusCode = STATUS_SUCCESS;
    char currentRealPath[PATH_MAX_LENGTH] = {0};
    char directoryFullPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_BUFF_SIZE] = {0};
    size_t responseLen = 0;
    // 1. 检查目录名是否为空
    if (task->data[0] == '\0') {
        log_error("mkdir: Empty directory name");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "mkdir error: directory name cannot be empty\n");
        goto send_response;
    }
    log_debug("Directory to create: %s", task->data);
    // 2. 转换虚拟路径到真实路径
    if (virtualPathToReal(currentVirtualPath, currentRealPath, sizeof(currentRealPath)) < 0) {
        log_error("Virtual path conversion failed: %s", currentVirtualPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "mkdir error: path conversion failed\n");
        goto send_response;
    }
    log_debug("Current real path: %s", currentRealPath);
    // 3. 获取目标目录的完整路径
    if (getDirectoryFullPath(currentRealPath, task->data, directoryFullPath) < 0) {
        log_error("Failed to get full path: %s", task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "mkdir error: invalid directory path\n");
        goto send_response;
    }
    log_debug("Full directory path: %s", directoryFullPath);
    // 4. 检查目录是否已存在
    struct stat st;
    if (stat(directoryFullPath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            log_warn("Directory already exists: %s", directoryFullPath);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response),
                                   "mkdir error: directory '%s' already exists\n", task->data);
        } else {
            log_error("Path exists but is not a directory: %s", directoryFullPath);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response),
                                   "mkdir error: '%s' exists but is not a directory\n", task->data);
        }
        goto send_response;
    }

    // 5. 创建目录 (0755 = rwxr-xr-x)
    if (mkdir(directoryFullPath, 0755) < 0) {
        log_error("mkdir failed: %s: %s", directoryFullPath, strerror(errno));
        statusCode = STATUS_FAIL;

        // 根据错误类型提供详细信息
        if (errno == EACCES) {
            responseLen = snprintf(response, sizeof(response), "mkdir error: permission denied\n");
        } else if (errno == ENOENT) {
            responseLen = snprintf(response, sizeof(response),
                                   "mkdir error: parent directory does not exist\n");
        } else if (errno == ENOSPC) {
            responseLen =
                snprintf(response, sizeof(response), "mkdir error: no space left on device\n");
        } else {
            responseLen =
                snprintf(response, sizeof(response), "mkdir error: %s\n", strerror(errno));
        }
        goto send_response;
    }

    // 6. 成功创建
    log_info("Directory created successfully: %s", directoryFullPath);
    responseLen =
        snprintf(response, sizeof(response), "Directory '%s' created successfully\n", task->data);

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("mkdir: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("mkdir command completed: status=%d, dir=%s", statusCode, task->data);
    }
}
/**
 * @brief 删除目录命令
 * @param task 任务结构
 */
void rmdirCommand(task_t *task) {
    log_info("Executing rmdir command (fd=%d)", task->peerFd);
    ResponseStatus statusCode = STATUS_SUCCESS;
    char currentRealPath[PATH_MAX_LENGTH] = {0};
    char directoryFullPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_BUFF_SIZE] = {0};
    size_t responseLen = 0;

    // 1. 检查目录名是否为空
    if (task->data[0] == '\0') {
        log_error("rmdir: Empty directory name");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "rmdir error: directory name cannot be empty\n");
        goto send_response;
    }

    log_debug("Directory to remove: %s", task->data);

    // 2. 检查是否试图删除当前目录或父目录
    if (strcmp(task->data, ".") == 0 || strcmp(task->data, "..") == 0) {
        log_error("rmdir: Cannot remove current or parent directory");
        statusCode = STATUS_INVALID_PARAM;
        responseLen = snprintf(response, sizeof(response),
                               "rmdir error: cannot remove '.' or '..' directory\n");
        goto send_response;
    }

    // 3. 转换虚拟路径到真实路径
    if (virtualPathToReal(currentVirtualPath, currentRealPath, sizeof(currentRealPath)) < 0) {
        log_error("Virtual path conversion failed: %s", currentVirtualPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "rmdir error: path conversion failed\n");
        goto send_response;
    }

    log_debug("Current real path: %s", currentRealPath);

    // 4. 获取目标目录的完整路径
    if (getDirectoryFullPath(currentRealPath, task->data, directoryFullPath) < 0) {
        log_error("Failed to get full path: %s", task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "rmdir error: invalid directory path\n");
        goto send_response;
    }

    log_debug("Full directory path: %s", directoryFullPath);

    // 5. 检查目录是否存在
    struct stat st;
    if (stat(directoryFullPath, &st) != 0) {
        log_error("Directory does not exist: %s", directoryFullPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "rmdir error: directory '%s' does not exist\n", task->data);
        goto send_response;
    }

    // 6. 检查是否为目录
    if (!S_ISDIR(st.st_mode)) {
        log_error("Target is not a directory: %s", directoryFullPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "rmdir error: '%s' is not a directory\n",
                               task->data);
        goto send_response;
    }

    // 7. 检查目录是否为空
    DIR *dir = opendir(directoryFullPath);
    if (dir == NULL) {
        log_error("Cannot open directory: %s: %s", directoryFullPath, strerror(errno));
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "rmdir error: cannot access directory '%s'\n", task->data);
        goto send_response;
    }

    struct dirent *entry;
    int isEmpty = 1;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 "." 和 ".." 条目
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            isEmpty = 0;
            break;
        }
    }
    closedir(dir);

    if (!isEmpty) {
        log_error("Directory not empty: %s", directoryFullPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "rmdir error: directory '%s' is not empty\n", task->data);
        goto send_response;
    }

    // 8. 删除目录
    if (rmdir(directoryFullPath) < 0) {
        log_error("rmdir failed: %s: %s", directoryFullPath, strerror(errno));
        statusCode = STATUS_FAIL;

        // 根据错误类型提供详细信息
        if (errno == EACCES) {
            responseLen = snprintf(response, sizeof(response), "rmdir error: permission denied\n");
        } else if (errno == EBUSY) {
            responseLen = snprintf(response, sizeof(response), "rmdir error: directory is busy\n");
        } else if (errno == ENOTEMPTY) {
            responseLen =
                snprintf(response, sizeof(response), "rmdir error: directory not empty\n");
        } else {
            responseLen =
                snprintf(response, sizeof(response), "rmdir error: %s\n", strerror(errno));
        }
        goto send_response;
    }

    // 9. 成功删除
    log_info("Directory removed successfully: %s", directoryFullPath);
    responseLen =
        snprintf(response, sizeof(response), "Directory '%s' removed successfully\n", task->data);

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("rmdir: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("rmdir command completed: status=%d, dir=%s", statusCode, task->data);
    }
}
/**
 * @brief        : 未识别命令处理
 * @param         {task_t} *task: 任务结构
 * @return        {*}
**/
void notCommand(task_t *task) {
    log_warn("Unknown command received (fd=%d)", task->peerFd);

    const char *helpText = "Error: Command not recognized.\n"
                           "Available commands:\n"
                           "  pwd     - Print working directory\n"
                           "  ls      - List directory contents\n"
                           "  cd      - Change directory\n"
                           "  mkdir   - Create directory\n"
                           "  rmdir   - Remove directory\n"
                           "  puts    - Upload file\n"
                           "  gets    - Download file\n";

    if (sendResponse(task->peerFd, STATUS_FAIL, DATA_TYPE_TEXT, helpText, strlen(helpText)) < 0) {
        log_error("notCommand: Failed to send help text");
    } else {
        log_info("Sent command help to client (fd=%d)", task->peerFd);
    }
}
/**
 * @brief 上传文件服务器的处理
 * @param task 处理的线程
 */
void putsCommand(task_t *task) {
    log_info("Executing puts command (fd=%d)", task->peerFd);

    // 接收文件传输头
    file_transfer_header_t header;
    int ret = recvn(task->peerFd, &header, sizeof(header));
    log_info("receive file header:fileSize=%u,offset=%u,mode=%d,filename=%s,checkSum=%s");
    if (ret != sizeof(header)) {
        log_error("Failed to receive transfer header");
        return;
    }
    // 转换网络字节序
    uint32_t fileSize = ntohl(header.fileSize);
    uint32_t offset = ntohl(header.offset);

    /**
     *     
    uint32_t fileSize;  // 文件总大小
    uint32_t offset;    // 传输起始偏移量
    TransferMode mode;  // 传输模式
    char filename[256]; // 文件名
    char checksum[33];  // MD5校验和（可选）
     */
    ResponseStatus statusCode = STATUS_SUCCESS;
    char fileDestVirtualPath[PATH_MAX_LENGTH] = {0}; // 文件目标虚拟路径
    char fileDestrealPath[PATH_MAX_LENGTH] = {0};    // 文件目标真实路径
    char fileName[FILENAME_LENGTH] = {0};            // 文件名称
    char fileFullPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_LENGTH] = {0};
    size_t responseLen = 0;
    int fd = -1;
    // task->data = test.txt destpath
    // 1. 检查文件名是否为空
    if (task->data[0] == '\0') {
        log_error("puts: Empty filename");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "puts error: filename cannot be empty\n");
        goto send_response;
    }

    // 2. 解析文件名（从路径中提取文件名）
    if (extractFilePathAndName(fileName, fileDestVirtualPath, task->data) < 0) {
        log_error("Failed to extract filename from: %s", task->data);
        strcpy(fileName, task->data); // 如果解析失败，直接使用原始数据作为文件名
    }

    log_info("File to upload: %s", fileName);

    // 3. 获取当前真实路径
    if (virtualPathToReal(fileDestVirtualPath, fileDestrealPath, sizeof(fileDestrealPath)) < 0) {
        log_error("Virtual path conversion failed: %s", fileDestVirtualPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "puts error: path conversion failed\n");
        goto send_response;
    }

    // 4. 构造完整文件路径
    if (getFileFullPath(fileName, fileDestrealPath, fileFullPath) < 0) {
        log_error("Failed to get full file path: %s", fileName);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "puts error: invalid file path\n");
        goto send_response;
    }

    log_debug("Full file path: %s", fileFullPath);
    // 根据传输模式打开文件
    if (header.mode == TRANSFER_MODE_RESUME && offset > 0) {
        // 断点续传模式：追加写入部分文件
        fd = open(fileFullPath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            log_error("Failed to open partial file for resume: %s", fileFullPath);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response), "Failed to resume transfer\n");
            goto send_response;
        }
        log_info("Resuming file transfer from offset: %u", offset);
    } else {
        // 正常模式：创建新的部分文件
        fd = open(fileFullPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            log_error("Failed to create partial file: %s", fileFullPath);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response), "Failed to create file\n");
            goto send_response;
        }
        offset = 0;
        log_info("Starting new file transfer");
    }

    // 8. 接收文件内容
    off_t totalReceived = offset;
    char buff[RECEIVE_FILE_BUFF_SIZE];

    while (totalReceived < fileSize) {
        off_t remaining = fileSize - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        ret = recvn(task->peerFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%d, received=%ld/%ld", ret, totalReceived,
                      fileSize);
            statusCode = STATUS_FAIL;
            responseLen =
                snprintf(response, sizeof(response), "puts error: file transfer interrupted\n");
            goto cleanup_file;
        }

        // 写入文件
        ssize_t written = write(fd, buff, ret);
        if (written != ret) {
            log_error("Failed to write file data: written=%zd, expected=%d", written, ret);
            statusCode = STATUS_FAIL;
            responseLen =
                snprintf(response, sizeof(response), "puts error: failed to write file\n");
            goto cleanup_file;
        }

        totalReceived += ret;
        log_debug("Received %ld/%ld bytes", totalReceived, fileSize);
    }
    // 10. 成功完成
    close(fd);
    fd = -1;
    // 9. 验证文件大小
    // 传输完成，将部分文件重命名为最终文件
    if (totalReceived == fileSize) {
        log_info("File transfer completed: %s (%u bytes)", fileFullPath, fileSize);
        responseLen =
            snprintf(response, sizeof(response), "File '%s' uploaded successfully (%u bytes)\n",
                     header.filename, fileSize);
    } else {
        log_error("File size mismatch: expected=%u, received=%u", fileSize, totalReceived);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "Transfer incomplete\n");
    }

    goto send_response;

cleanup_file:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("puts: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("puts command completed: status=%d, file=%s", statusCode, fileName);
    }

    // 重新添加到 epoll 监听
    addEpollReadFd(task->epFd, task->peerFd);
}
/**
 * @brief 下载文件的命令
 * @param task 对应线程
 */
void getsCommand(task_t *task) {
    log_info("Executing gets command (fd=%d)", task->peerFd);
    log_info("gets file path: %s", task->data);
    char response[RESPONSE_LENGTH] = {0};
    char fileRealPath[PATH_MAX_LENGTH] = {0};
    char fileVirtualPath[PATH_MAX_LENGTH] = {0};
    ResponseStatus statusCode = STATUS_SUCCESS;
    int responseLen = 0;
    uint32_t clientOffset = 0;
    // 解析请求: "virtualPath|offset"
    if (sscanf(task->data, "%[^|]|%u", fileVirtualPath, &clientOffset) < 1) {
        log_error("Invalid gets request format: %s", task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: invalid request format\n");
        goto send_response;
    }
    log_info("Gets request: path=%s, offset=%u", fileVirtualPath, clientOffset);

    if (virtualPathToReal(fileVirtualPath, fileRealPath, sizeof(fileRealPath)) < 0) {
        log_error("Virtual path conversion failed: %s", fileVirtualPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: path conversion failed\n");
        goto send_response;
    }
    log_info("File real path=%s", fileRealPath);

    // 检查文件存在且为普通文件
    struct stat st;
    if (stat(fileRealPath, &st) != 0) {
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "file get error(unexist).");
        goto send_response;
    } else if (!S_ISREG(st.st_mode)) {
        statusCode = STATUS_FAIL;
        responseLen =
            snprintf(response, sizeof(response), "file get error(maby it is a directory).");
        goto send_response;
    }

    // 保存原始文件大小（主机字节序）
    uint32_t totalFileSize = (uint32_t)st.st_size;
    // 验证 offset 合法性
    if (clientOffset > totalFileSize) {
        log_error("Invalid offset: %u > file size %u", clientOffset, totalFileSize);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: invalid offset\n");
        goto send_response;
    }
    // 计算需要发送的数据量
    uint32_t dataToSend = totalFileSize - clientOffset;
    // 提取文件名
    const char *filename = strrchr(fileRealPath, '/');
    filename = filename ? filename + 1 : fileRealPath;
    char checkSum[33];
    calculateFileMD5(fileRealPath, checkSum);
    if (sendFileHeader(task->peerFd, totalFileSize, clientOffset, filename, checkSum)
        != sizeof(file_transfer_header_t)) {
        log_error("Failed to send file header");
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: send header failed\n");
        goto send_response;
    }

    // 如果客户端已经有完整文件，直接返回成功
    if (dataToSend == 0) {
        log_info("File already complete on client side");
        responseLen = snprintf(response, sizeof(response), "File already downloaded (%u bytes).\n",
                               totalFileSize);
        goto send_response;
    }
    // 打开文件
    int fileFd = open(fileRealPath, O_RDONLY);
    if (fileFd < 0) {
        log_error("Failed to open file: %s", fileRealPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "file open error.");
        goto send_response;
    }
    // 跳转到 offset 位置
    if (clientOffset > 0) {
        if (lseek(fileFd, clientOffset, SEEK_SET) != (off_t)clientOffset) {
            log_error("Failed to seek to offset %u", clientOffset);
            close(fileFd);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response), "gets error: seek failed\n");
            goto send_response;
        }
        log_info("Resuming download from offset: %u", clientOffset);
    }

    char buff[SEND_FILE_BUFF_SIZE];
    uint32_t totalSend = 0;

    while (totalSend < dataToSend) {
        ssize_t bytesRead = read(fileFd, buff, sizeof(buff));
        if (bytesRead <= 0) {
            if (bytesRead < 0) {
                log_error("read file error, bytesRead=%zd", bytesRead);
            }
            close(fileFd);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response), "file read error.");
            goto send_response;
        }

        ssize_t bytesSend = sendn(task->peerFd, buff, bytesRead);
        if (bytesSend != bytesRead) {
            log_error("send file data error, bytesSend!=bytesRead");
            close(fileFd);
            statusCode = STATUS_FAIL;
            responseLen = snprintf(response, sizeof(response), "file send error.");
            goto send_response;
        }
        totalSend += bytesSend;
    }
    close(fileFd);

    // 验证传输完整性
    if (totalSend != dataToSend) {
        log_error("File transfer incomplete: %u/%u bytes", totalSend, dataToSend);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "file transfer incomplete.");
        goto send_response;
    }

    // 成功发送文件
    if (clientOffset > 0) {
        responseLen =
            snprintf(response, sizeof(response),
                     "File resumed and downloaded successfully (%u bytes, from offset %u).\n",
                     totalSend, clientOffset);
    } else {
        responseLen = snprintf(response, sizeof(response),
                               "File download successfully (%u bytes).\n", totalSend);
    }
send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("gets: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("gets command completed: status=%d, file path=%s", statusCode, task->data);
    }
    // addEpollReadFd(task->epFd, task->peerFd);
    // log_info("gets: fd %d re-added to epoll", task->peerFd);
}
/**
 * @brief 校验用户密码，
 * @param task 对应的线程
 */
void userLoginVerifyUsername(task_t *task) {
    log_info("username login Verify username(%s).", task->data);
    ListNode *pNode = userList;
    char response[RESPONSE_LENGTH] = {0};
    int found = 0;

    while (pNode != NULL) {
        user_info_t *user = (user_info_t *)pNode->val;
        if (user->sockfd == task->peerFd) {
            found = 1;
            strcpy(user->name, task->data);
            int ret = selectUsername(user, response);
            if (0 == ret) {
                log_info("Username '%s' found, sending salt", task->data);
                sendResponse(user->sockfd, STATUS_SUCCESS, DATA_TYPE_CIPHERTEXT, response,
                             strlen(response));
            } else {
                log_warn("Username '%s' not found in system", task->data);
                int responseLen =
                    snprintf(response, sizeof(response), "no such username was found.\n");
                sendResponse(user->sockfd, STATUS_FAIL, DATA_TYPE_TEXT, response, responseLen);
            }
            return;
        }
        pNode = pNode->next;
    }
    if (!found) {
        log_error("User node not found for sockfd=%d", task->peerFd);
        int responseLen =
            snprintf(response, sizeof(response), "Internal error: user session not found.\n");
        sendResponse(task->peerFd, STATUS_FAIL, DATA_TYPE_TEXT, response, responseLen);
    }
}
/**
 * @brief 校验用户密码，
 * @param task 对应的线程
 */
void userLoginVerifyPassword(task_t *task) {
    log_info("Password verification for cryptograph: %s", task->data);
    ListNode *pNode = userList;
    char response[RESPONSE_LENGTH] = {0};
    int found = 0;
    int responseLen = 0;

    while (pNode != NULL) {
        user_info_t *user = (user_info_t *)pNode->val;
        if (user->sockfd == task->peerFd) {
            found = 1;
            if (strcmp(task->data, user->encrypted) == 0) {
                user->status = STATUS_LOGIN;
                strcpy(user->pwd, "/");
                user->login_time = time(NULL);
                log_info("User '%s' logged in successfully at %ld", user->name, user->login_time);
                responseLen = snprintf(response, sizeof(response),
                                       "Login successful! Welcome, %s\n", user->name);
                if (sendResponse(user->sockfd, STATUS_SUCCESS, DATA_TYPE_TEXT, response,
                                 responseLen)
                    < 0) {
                    log_error("Failed to send login success message");
                    return;
                }
                responseLen = strlen(user->pwd);
                if (sendResponse(user->sockfd, STATUS_SUCCESS, DATA_TYPE_TEXT, user->pwd,
                                 responseLen)
                    < 0) {
                    log_error("Failed to send current working directory");
                    return;
                }
                log_info("Login response sent: success message + pwd (%s)", user->pwd);
            } else {
                log_warn("Password verification failed for user '%s' (fd=%d)", user->name,
                         user->sockfd);
                responseLen =
                    snprintf(response, sizeof(response), "Incorrect password. Please try again.\n");
                sendResponse(user->sockfd, STATUS_FAIL, DATA_TYPE_TEXT, response, responseLen);
            }
            return;
        }
        pNode = pNode->next;
    }
    // 未找到用户节点
    if (!found) {
        log_error("User node not found for sockfd=%d", task->peerFd);
        responseLen =
            snprintf(response, sizeof(response), "Internal error: user session not found.\n");
        sendResponse(task->peerFd, STATUS_FAIL, DATA_TYPE_TEXT, response, responseLen);
    }
}
/**
 * @brief        : 执行命令
 * @param         {task_t} *task: 对应的任务
 * @return        {void}
**/
void executeCmd(task_t *task) {
    switch (task->type) {
        case CMD_TYPE_PWD:
            pwdCommand(task);
            break;
        case CMD_TYPE_CD:
            cdCommand(task);
            break;
        case CMD_TYPE_LS:
            lsCommand(task);
            break;
        case CMD_TYPE_MKDIR:
            mkdirCommand(task);
            break;
        case CMD_TYPE_RMDIR:
            rmdirCommand(task);
            break;
        case CMD_TYPE_NOTCMD:
            notCommand(task);
            break;
        case CMD_TYPE_PUTS:
            putsCommand(task);
            break;
        case CMD_TYPE_GETS:
            getsCommand(task);
            break;
        case CMD_TYPE_CHECK_PARTIAL:
            checkPartialFile(task);
            break;
        case TASK_LOGIN_VERIFY_USERNAME:
            userLoginVerifyUsername(task);
            break;
        case TASK_LOGIN_VERIFY_PASSWORD:
            userLoginVerifyPassword(task);
            break;
    }
}