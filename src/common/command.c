/**
 * @FilePath     : /CloudDisk/src/common/command.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-27 22:16:29
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
#include "select.h"
#include <sys/mman.h>
extern ListNode *userList;
static char currentVirtualPath[PATH_MAX_LENGTH] = "/";

/**
 * @brief 获取链表中的一个节点
 * @param val 指定节点对应的值 
 * @param head 链表头指针
 * @return 指定节点指针或NULL
 */
user_info_t *getListUser(int sockfd) {
    /**
     * user_info_t *user = (user_info_t *)pNode->val;
     */
    ListNode *cur = userList;
    while (cur) {
        user_info_t *user = (user_info_t *)cur->val;
        if (user->sockfd == sockfd) {
            return user;
        }
        cur = cur->next;
    }
    return NULL;
}
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
    uint64_t uploadedSize = selectUploadTask(filename, checksum);
    if (uploadedSize == 0) {
        statusCode = STATUS_FAIL;
        strcpy(response, "0");
        goto send_response;
    }
    snprintf(response, sizeof(response), "%" PRIu64, uploadedSize);
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
 * @brief 使用 mmap 接收大文件
 * @return 0 成功，-1 失败
 */
static int receiveFileWithMmap(int sockFd, const char *filePath, uint32_t fileSize, uint32_t offset,
                               char *response, size_t responseSize) {
    int fd = -1;
    void *mapped = MAP_FAILED;
    int result = -1;

    // 计算需要接收的数据量
    uint32_t dataToReceive = fileSize - offset;

    // 打开/创建文件
    if (offset > 0) {
        fd = open(filePath, O_RDWR);
    } else {
        fd = open(filePath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }

    if (fd < 0) {
        log_error("Failed to open file for mmap: %s", filePath);
        snprintf(response, responseSize, "puts error: failed to open file\n");
        return -1;
    }

    // 扩展文件到目标大小
    if (ftruncate(fd, fileSize) < 0) {
        log_error("Failed to extend file: %s", strerror(errno));
        snprintf(response, responseSize, "puts error: failed to allocate space\n");
        goto cleanup;
    }

    // 映射文件到内存（从 offset 开始的部分）
    mapped = mmap(NULL, dataToReceive, PROT_WRITE, MAP_SHARED, fd, offset);
    if (mapped == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        snprintf(response, responseSize, "puts error: mmap failed\n");
        goto cleanup;
    }

    log_info("File mapped successfully: size=%u, offset=%u, dataToReceive=%u", fileSize, offset,
             dataToReceive);

    // 直接接收数据到映射内存
    uint32_t totalReceived = 0;
    char *writePtr = (char *)mapped;

    while (totalReceived < dataToReceive) {
        uint32_t remaining = dataToReceive - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        ssize_t ret = recvn(sockFd, writePtr, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%zd, received=%u/%u", ret, totalReceived,
                      dataToReceive);
            snprintf(response, responseSize, "puts error: file transfer interrupted\n");
            goto cleanup;
        }

        writePtr += ret;
        totalReceived += ret;

        // log_debug("Received %u/%u bytes (mmap mode)", totalReceived, dataToReceive);
    }

    // 同步到磁盘
    if (msync(mapped, dataToReceive, MS_SYNC) < 0) {
        log_warn("msync failed: %s", strerror(errno));
    }

    log_info("File transfer completed (mmap): %s (%u bytes)", filePath, fileSize);
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
 * @brief 使用 mmap 接收大文件
 * @return 0 成功，-1 失败
 */
static int receiveFileWithMmapVirtual(int sockFd, const char *filePath,
                                      file_transfer_header_t *file_header, char *response,
                                      size_t responseSize) {
    int fd = -1;
    void *mapped = MAP_FAILED;
    int result = -1;
    // 计算需要接收的数据量
    uint32_t dataToReceive = file_header->fileSize - file_header->offset;
    log_info("receive file data %d", dataToReceive);
    // 打开/创建文件
    if (file_header->offset > 0) {
        fd = open(filePath, O_RDWR);
    } else {
        fd = open(filePath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }
    if (fd < 0) {
        log_error("Failed to open file for mmap: %s", filePath);
        snprintf(response, responseSize, "puts error: failed to open file\n");
        return -1;
    }
    // 扩展文件到目标大小
    if (ftruncate(fd, file_header->fileSize) < 0) {
        log_error("Failed to extend file: %s", strerror(errno));
        snprintf(response, responseSize, "puts error: failed to allocate space\n");
        goto cleanup;
    }
    // 映射文件到内存（从 offset 开始的部分）
    mapped = mmap(NULL, dataToReceive, PROT_WRITE, MAP_SHARED, fd, file_header->offset);
    if (mapped == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        snprintf(response, responseSize, "puts error: mmap failed\n");
        goto cleanup;
    }
    log_info("File mapped successfully: size=%u, offset=%u, dataToReceive=%u",
             file_header->fileSize, file_header->offset, dataToReceive);
    // 直接接收数据到映射内存
    uint32_t totalReceived = 0;
    char *writePtr = (char *)mapped;
    while (totalReceived < dataToReceive) {
        uint32_t remaining = dataToReceive - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;
        ssize_t ret = recvn(sockFd, writePtr, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%zd, received=%u/%u", ret, totalReceived,
                      dataToReceive);
            insertUploadTask(file_header, totalReceived);
            snprintf(response, responseSize, "puts error: file transfer interrupted\n");
            goto cleanup;
        }
        writePtr += ret;
        totalReceived += ret;
        // log_debug("Received %u/%u bytes (mmap mode)", totalReceived, dataToReceive);
    }
    // 同步到磁盘
    if (msync(mapped, dataToReceive, MS_SYNC) < 0) {
        log_warn("msync failed: %s", strerror(errno));
    }
    log_info("File transfer completed (mmap): %s (%u bytes)", filePath, file_header->fileSize);
    result = 0;
    if (file_header->offset > 0) {
        if (deleteUploadTask(file_header) < 0) {
            log_error("delete from t_upload_task error.");
        }
    }
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
 * @brief 使用传统方式接收文件
 * @return 0 成功，-1 失败
 */
static int receiveFileTraditionalVirtual(int sockFd, const char *filePath,
                                         file_transfer_header_t *file_header, char *response,
                                         size_t responseSize) {
    int fd = -1;
    int result = -1;
    // 根据传输模式打开文件
    if (file_header->mode == TRANSFER_MODE_RESUME && file_header->offset > 0) {
        fd = open(filePath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            log_error("Failed to open partial file for resume: %s", filePath);
            snprintf(response, responseSize, "Failed to resume transfer\n");
            return -1;
        }
        log_info("Resuming file transfer from offset: %u", file_header->offset);
    } else {
        fd = open(filePath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            log_error("Failed to create file: %s", filePath);
            snprintf(response, responseSize, "Failed to create file\n");
            return -1;
        }
        file_header->offset = 0;
        log_info("Starting new file transfer");
    }

    // 接收文件内容
    uint32_t totalReceived = file_header->offset;
    char buff[RECEIVE_FILE_BUFF_SIZE];

    while (totalReceived < file_header->fileSize) {
        uint32_t remaining = file_header->fileSize - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;

        ssize_t ret = recvn(sockFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%zd, received=%u/%u", ret, totalReceived,
                      file_header->fileSize);
            snprintf(response, responseSize, "puts error: file transfer interrupted\n");
            goto cleanup;
        }
        ssize_t written = write(fd, buff, ret);
        if (written != ret) {
            log_error("Failed to write file data: written=%zd, expected=%zd", written, ret);
            snprintf(response, responseSize, "puts error: failed to write file\n");
            goto cleanup;
        }
        totalReceived += ret;
        // log_debug("Received %u/%u bytes", totalReceived, file_header->fileSize);
    }
    result = 0;
cleanup:
    if (fd >= 0) {
        close(fd);
    }
    return result;
}
/**
 * @brief 使用传统方式接收文件
 * @return 0 成功，-1 失败
 */
static int receiveFileTraditional(int sockFd, const char *filePath, uint32_t fileSize,
                                  uint32_t offset, TransferMode mode, char *response,
                                  size_t responseSize) {
    int fd = -1;
    int result = -1;
    // 根据传输模式打开文件
    if (mode == TRANSFER_MODE_RESUME && offset > 0) {
        fd = open(filePath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            log_error("Failed to open partial file for resume: %s", filePath);
            snprintf(response, responseSize, "Failed to resume transfer\n");
            return -1;
        }
        log_info("Resuming file transfer from offset: %u", offset);
    } else {
        fd = open(filePath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            log_error("Failed to create file: %s", filePath);
            snprintf(response, responseSize, "Failed to create file\n");
            return -1;
        }
        offset = 0;
        log_info("Starting new file transfer");
    }
    // 接收文件内容
    uint32_t totalReceived = offset;
    char buff[RECEIVE_FILE_BUFF_SIZE];
    while (totalReceived < fileSize) {
        uint32_t remaining = fileSize - totalReceived;
        size_t toReceive =
            (remaining < RECEIVE_FILE_BUFF_SIZE) ? remaining : RECEIVE_FILE_BUFF_SIZE;
        ssize_t ret = recvn(sockFd, buff, toReceive);
        if (ret <= 0) {
            log_error("Failed to receive file data: ret=%zd, received=%u/%u", ret, totalReceived,
                      fileSize);
            snprintf(response, responseSize, "puts error: file transfer interrupted\n");
            goto cleanup;
        }
        ssize_t written = write(fd, buff, ret);
        if (written != ret) {
            log_error("Failed to write file data: written=%zd, expected=%zd", written, ret);
            snprintf(response, responseSize, "puts error: failed to write file\n");
            goto cleanup;
        }
        totalReceived += ret;
        log_debug("Received %u/%u bytes", totalReceived, fileSize);
    }
    result = 0;
cleanup:
    if (fd >= 0) {
        close(fd);
    }
    return result;
}
/**
 * @brief 上传文件服务器的处理(虚拟文件系统版本)
 * @param task 处理的线程
 */
void putsCommandVirtual(task_t *task) {
    log_info("Executing puts command (virtual, fd=%d)", task->peerFd);
    char response[RESPONSE_LENGTH];
    char fileFullPath[PATH_MAX_LENGTH] = {0};
    char fileName[FILENAME_LENGTH] = {0};
    char destPath[PATH_MAX_LENGTH] = {0};
    ResponseStatus statusCode = STATUS_SUCCESS;
    ssize_t responseLen = 0;
    // 1. 获取用户信息
    user_info_t *user = getListUser(task->peerFd);
    if (!user) {
        log_error("puts: User not found for fd=%d", task->peerFd);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "puts error: user session not found\n");
        goto send_response;
    }
    // 2. 解析文件名和目标路径 (task->data = "filename destpath")
    if (task->data[0] == '\0') {
        log_error("puts: Empty command data");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "puts error: filename cannot be empty\n");
        goto send_response;
    }
    // 提取文件名和目标路径
    if (extractFilePathAndName(fileName, destPath, task->data) < 0) {
        log_error("Failed to extract filename from: %s", task->data);
        strncpy(fileName, task->data, sizeof(fileName) - 1);
        strcpy(destPath, user->pwd); // 默认使用当前目录
    }
    log_info("File to upload: %s -> %s", fileName, destPath);
    // 3. 解析目标路径
    char parsePath[PATH_MAX_SEGMENTS][PATH_SEGMENT_MAX_LENGTH];
    int pathCount = parsePathToArray(destPath, parsePath, user->pwd);
    if (pathCount < 0) {
        log_error("puts: Path parsing failed: %s", destPath);
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "puts error: invalid destination path\n");
        goto send_response;
    }
    // 4. 确保目标目录存在(不存在则创建)
    int targetDirectoryId = resolveOrCreateDirectory(user, parsePath, pathCount);
    if (targetDirectoryId < 0) {
        log_error("puts: Failed to resolve/create directory: %s", destPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "puts error: failed to access destination directory\n");
        goto send_response;
    }
    log_info("Target directory ID: %d", targetDirectoryId);
    // 5. 接收文件传输头
    file_transfer_header_t header;
    int ret = recvn(task->peerFd, &header, sizeof(header));
    if (ret != sizeof(header)) {
        log_error("Failed to receive transfer header");
        addEpollReadFd(task->epFd, task->peerFd);
        return;
    }
    // 6. 转换网络字节序
    uint32_t fileSize = ntohl(header.fileSize);
    uint32_t offset = ntohl(header.offset);
    header.fileSize = ntohl(header.fileSize);
    header.offset = ntohl(header.offset);
    log_info("Received file header: fileSize=%u, offset=%u, mode=%d, filename=%s, checkSum=%s",
             fileSize, offset, header.mode, header.filename, header.checksum);
    // 7. 检查文件是否已存在(秒传逻辑)
    file_t existingFile;
    memset(&existingFile, 0, sizeof(file_t));
    strncpy(existingFile.md5, header.checksum, sizeof(existingFile.md5) - 1);
    int fileExists = selectFileByMd5(&existingFile);
    if (fileExists == 0) {
        // 文件已存在,执行秒传
        log_info("File already exists (MD5=%s), performing instant upload", header.checksum);

        // 创建虚拟文件系统记录(引用已存在的物理文件)
        file_t newFile;
        memset(&newFile, 0, sizeof(file_t));
        newFile.parent_id = (uint32_t)targetDirectoryId;
        strncpy(newFile.filename, fileName, sizeof(newFile.filename) - 1);
        newFile.owner_id = user->user_id;
        strncpy(newFile.md5, header.checksum, sizeof(newFile.md5) - 1);
        newFile.file_size = fileSize;
        newFile.type = 1; // 普通文件

        if (insertFile(&newFile) == 0) {
            responseLen = snprintf(response, sizeof(response),
                                   "File '%s' uploaded instantly (already exists, %u bytes)\n",
                                   fileName, fileSize);
            log_info("Instant upload completed: file_id=%u", newFile.id);
        } else {
            statusCode = STATUS_FAIL;
            responseLen =
                snprintf(response, sizeof(response), "puts error: failed to create file record\n");
        }
        goto send_response;
    }
    // 8. 构造物理文件路径(使用 MD5 作为文件名存储)
    if (getFileFullPath(header.checksum, CLOUD_DISK_ROOT, fileFullPath) < 0) {
        log_error("Failed to get full file path for MD5: %s", header.checksum);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "puts error: invalid file path\n");
        goto send_response;
    }
    log_debug("Physical file path: %s", fileFullPath);
    // 9. 接收文件数据
    int transferResult;
    uint32_t dataToReceive = fileSize - offset;
    if (dataToReceive >= MMAP_THRESHOLD) {
        log_info("Using mmap for large file transfer (%u bytes)", dataToReceive);
        transferResult = receiveFileWithMmapVirtual(task->peerFd, fileFullPath, &header, response,
                                                    sizeof(response));
    } else {
        log_info("Using traditional method for file transfer (%u bytes)", dataToReceive);
        transferResult = receiveFileTraditionalVirtual(task->peerFd, fileFullPath, &header,
                                                       response, sizeof(response));
    }
    if (transferResult < 0) {
        statusCode = STATUS_FAIL;
        responseLen = strlen(response);
        goto send_response;
    }
    // 10. 验证文件完整性(可选,建议添加)
    char calculatedMD5[33];
    if (calculateFileMD5(fileFullPath, calculatedMD5) == 0) {
        if (strcmp(calculatedMD5, header.checksum) != 0) {
            log_error("MD5 mismatch: expected=%s, got=%s", header.checksum, calculatedMD5);
            statusCode = STATUS_FAIL;
            responseLen =
                snprintf(response, sizeof(response), "puts error: file integrity check failed\n");
            unlink(fileFullPath); // 删除损坏的文件
            goto send_response;
        }
    }

    // 11. 插入数据库记录
    file_t newFile;
    memset(&newFile, 0, sizeof(file_t));
    newFile.parent_id = (uint32_t)targetDirectoryId;
    strncpy(newFile.filename, fileName, sizeof(newFile.filename) - 1);
    newFile.owner_id = user->user_id;
    strncpy(newFile.md5, header.checksum, sizeof(newFile.md5) - 1);
    newFile.file_size = fileSize;
    newFile.type = 1; // 普通文件

    if (insertFile(&newFile) != 0) {
        log_error("Failed to insert file record into database");
        statusCode = STATUS_FAIL;
        responseLen =
            snprintf(response, sizeof(response), "puts error: failed to create file record\n");
        unlink(fileFullPath); // 删除物理文件
        goto send_response;
    }

    // 12. 成功响应
    if (offset > 0) {
        responseLen =
            snprintf(response, sizeof(response),
                     "File '%s' resumed and uploaded successfully (%u bytes, from offset %u)\n",
                     fileName, fileSize, offset);
    } else {
        responseLen = snprintf(response, sizeof(response),
                               "File '%s' uploaded successfully (%u bytes)\n", fileName, fileSize);
    }
    log_info("File uploaded: id=%u, name=%s, size=%u, md5=%s", newFile.id, fileName, fileSize,
             header.checksum);
send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("puts: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("puts command completed: status=%d, file=%s", statusCode, fileName);
    }

    addEpollReadFd(task->epFd, task->peerFd);
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
    if (ret != sizeof(header)) {
        log_error("Failed to receive transfer header");
        // 无法发送响应，因为可能连接已断开
        addEpollReadFd(task->epFd, task->peerFd);
        return;
    }
    // 转换网络字节序
    uint32_t fileSize = ntohl(header.fileSize);
    uint32_t offset = ntohl(header.offset);
    log_info("Received file header: fileSize=%u, offset=%u, mode=%d, filename=%s, checkSum=%s",
             fileSize, offset, header.mode, header.filename, header.checksum);

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
    log_info("File to upload: %s -> %s", fileName, fileDestVirtualPath);
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
    // 5. 根据文件大小选择传输方式
    int transferResult;
    uint32_t dataToReceive = fileSize - offset;

    if (dataToReceive >= MMAP_THRESHOLD) {
        // 大文件：使用 mmap
        log_info("Using mmap for large file transfer (%u bytes)", dataToReceive);
        transferResult = receiveFileWithMmap(task->peerFd, fileFullPath, fileSize, offset, response,
                                             sizeof(response));
    } else {
        // 小文件：使用传统方式
        log_info("Using traditional method for file transfer (%u bytes)", dataToReceive);
        transferResult = receiveFileTraditional(task->peerFd, fileFullPath, fileSize, offset,
                                                header.mode, response, sizeof(response));
    }
    if (transferResult < 0) {
        statusCode = STATUS_FAIL;
        responseLen = strlen(response);
        goto send_response;
    }
    if (offset > 0) {
        responseLen =
            snprintf(response, sizeof(response),
                     "File '%s' resumed and uploaded successfully (%u bytes, from offset %u)\n",
                     header.filename, fileSize, offset);
    } else {
        responseLen =
            snprintf(response, sizeof(response), "File '%s' uploaded successfully (%u bytes)\n",
                     header.filename, fileSize);
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
 * @brief 使用 mmap 发送大文件
 * @return 0 成功，-1 失败
 */
static int sendFileWithMmap(int sockFd, const char *filePath, uint32_t fileSize, uint32_t offset,
                            char *response, size_t responseSize) {
    int fd = -1;
    void *mapped = MAP_FAILED;
    int result = -1;

    uint32_t dataToSend = fileSize - offset;

    fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file for mmap: %s", filePath);
        snprintf(response, responseSize, "gets error: failed to open file\n");
        return -1;
    }

    // 映射文件到内存
    mapped = mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        log_error("mmap failed: %s", strerror(errno));
        snprintf(response, responseSize, "gets error: mmap failed\n");
        goto cleanup;
    }

    // 建议内核顺序读取
    madvise(mapped, fileSize, MADV_SEQUENTIAL);

    log_info("File mapped successfully: size=%u, offset=%u, dataToSend=%u", fileSize, offset,
             dataToSend);

    // 从映射内存发送数据
    uint32_t totalSent = 0;
    const char *readPtr = (const char *)mapped + offset;

    while (totalSent < dataToSend) {
        uint32_t remaining = dataToSend - totalSent;
        size_t toSend = (remaining < SEND_FILE_BUFF_SIZE) ? remaining : SEND_FILE_BUFF_SIZE;

        ssize_t ret = sendn(sockFd, readPtr, toSend);
        if (ret != (ssize_t)toSend) {
            log_error("Failed to send file data: ret=%zd, expected=%zu", ret, toSend);
            snprintf(response, responseSize, "gets error: file transfer failed\n");
            goto cleanup;
        }

        readPtr += ret;
        totalSent += ret;

        // log_debug("Sent %u/%u bytes (mmap mode)", totalSent, dataToSend);
    }

    log_info("File transfer completed (mmap): %s (%u bytes sent)", filePath, totalSent);
    result = 0;

cleanup:
    if (mapped != MAP_FAILED) {
        munmap(mapped, fileSize);
    }
    if (fd >= 0) {
        close(fd);
    }
    return result;
}

/**
 * @brief 使用传统方式发送文件
 */
static int sendFileTraditional(int sockFd, const char *filePath, uint32_t fileSize, uint32_t offset,
                               char *response, size_t responseSize) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file: %s", filePath);
        snprintf(response, responseSize, "gets error: file open error\n");
        return -1;
    }

    // 跳转到 offset 位置
    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) != (off_t)offset) {
            log_error("Failed to seek to offset %u", offset);
            close(fd);
            snprintf(response, responseSize, "gets error: seek failed\n");
            return -1;
        }
        log_info("Resuming download from offset: %u", offset);
    }

    uint32_t dataToSend = fileSize - offset;
    uint32_t totalSent = 0;
    char buff[SEND_FILE_BUFF_SIZE];

    while (totalSent < dataToSend) {
        ssize_t bytesRead = read(fd, buff, sizeof(buff));
        if (bytesRead <= 0) {
            log_error("read file error, bytesRead=%zd", bytesRead);
            close(fd);
            snprintf(response, responseSize, "gets error: file read error\n");
            return -1;
        }

        ssize_t bytesSent = sendn(sockFd, buff, bytesRead);
        if (bytesSent != bytesRead) {
            log_error("send file data error, bytesSent=%zd, bytesRead=%zd", bytesSent, bytesRead);
            close(fd);
            snprintf(response, responseSize, "gets error: file send error\n");
            return -1;
        }

        totalSent += bytesSent;
        log_debug("Sent %u/%u bytes", totalSent, dataToSend);
    }

    close(fd);
    return 0;
}
/**
 * @brief 下载文件的命令
 * @param task 对应线程
 */
void getsCommandVirtual(task_t *task) {
    log_info("Executing gets command (fd=%d)", task->peerFd);
    log_info("gets file path: %s", task->data);
    char response[RESPONSE_LENGTH] = {0};
    char fileVirtualPath[PATH_MAX_LENGTH] = {0};
    ResponseStatus statusCode = STATUS_SUCCESS;
    int responseLen = 0;
    uint32_t clientOffset = 0;
    user_info_t *user = getListUser(task->peerFd);
    // 解析请求: "virtualPath|offset"
    if (sscanf(task->data, "%[^|]|%u", fileVirtualPath, &clientOffset) < 1) {
        log_error("Invalid gets request format: %s", task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: invalid request format\n");
        goto send_response;
    }
    log_info("Gets request: path=%s, offset=%u", fileVirtualPath, clientOffset);
    // 3. 解析目标路径
    char parsePath[PATH_MAX_SEGMENTS][PATH_SEGMENT_MAX_LENGTH];
    int pathCount = parsePathToArray(fileVirtualPath, parsePath, user->pwd);
    file_t destFile;
    int ret = selectFileByPath(user, parsePath, pathCount, &destFile);
    if (ret < 0) {
        log_error("no such file: %s", destFile.filename);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: no such file.\n");
        goto send_response;
    }
    if (destFile.type == 2) {
        log_error("can't gets a directory: %s", destFile.filename);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "can't gets a directory.\n");
        goto send_response;
    }
    // 保存原始文件大小（主机字节序）
    uint32_t totalFileSize = (uint32_t)destFile.file_size;
    // 验证 offset 合法性
    if (clientOffset > totalFileSize) {
        log_error("Invalid offset: %u > file size %u", clientOffset, totalFileSize);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: invalid offset\n");
        goto send_response;
    }
    // 计算需要发送的数据量
    uint32_t dataToSend = totalFileSize - clientOffset;
    if (sendFileHeader(task->peerFd, totalFileSize, clientOffset, destFile.filename, destFile.md5)
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
    char fileRealPath[PATH_MAX_LENGTH];
    if (getFileFullPath(destFile.md5, CLOUD_DISK_ROOT, fileRealPath) < 0) {
        log_error("Failed to get full file path for MD5: %s", destFile.md5);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "gets error: invalid file path\n");
        goto send_response;
    }
    log_info("file real path is %s",fileRealPath);
    // 打开文件
    int transferResult;
    if (dataToSend >= MMAP_THRESHOLD) {
        log_info("Using mmap for large file transfer (%u bytes)", dataToSend);
        transferResult = sendFileWithMmap(task->peerFd, fileRealPath, totalFileSize, clientOffset,
                                          response, sizeof(response));
    } else {
        log_info("Using traditional method for file transfer (%u bytes)", dataToSend);
        transferResult = sendFileTraditional(task->peerFd, fileRealPath, totalFileSize,
                                             clientOffset, response, sizeof(response));
    }
    if (transferResult < 0) {
        statusCode = STATUS_FAIL;
        responseLen = strlen(response);
        goto send_response;
    }
    // 8. 成功
    if (clientOffset > 0) {
        responseLen =
            snprintf(response, sizeof(response),
                     "File resumed and downloaded successfully (%u bytes, from offset %u).\n",
                     dataToSend, clientOffset);
    } else {
        responseLen = snprintf(response, sizeof(response),
                               "File download successfully (%u bytes).\n", dataToSend);
    }
send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("gets: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("gets command completed: status=%d, file path=%s", statusCode, task->data);
    }
    addEpollReadFd(task->epFd, task->peerFd);
    // log_info("gets: fd %d re-added to epoll", task->peerFd);
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
    int transferResult;
    if (dataToSend >= MMAP_THRESHOLD) {
        log_info("Using mmap for large file transfer (%u bytes)", dataToSend);
        transferResult = sendFileWithMmap(task->peerFd, fileRealPath, totalFileSize, clientOffset,
                                          response, sizeof(response));
    } else {
        log_info("Using traditional method for file transfer (%u bytes)", dataToSend);
        transferResult = sendFileTraditional(task->peerFd, fileRealPath, totalFileSize,
                                             clientOffset, response, sizeof(response));
    }
    if (transferResult < 0) {
        statusCode = STATUS_FAIL;
        responseLen = strlen(response);
        goto send_response;
    }
    // 8. 成功
    if (clientOffset > 0) {
        responseLen =
            snprintf(response, sizeof(response),
                     "File resumed and downloaded successfully (%u bytes, from offset %u).\n",
                     dataToSend, clientOffset);
    } else {
        responseLen = snprintf(response, sizeof(response),
                               "File download successfully (%u bytes).\n", dataToSend);
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
 * @brief 用户成功登入初始化
 * @param user 登入的用户
 */
void userSuccessLoginInit(user_info_t *user) {
    user->status = STATUS_LOGIN;
    strcpy(user->pwd, "/");
    user->login_time = time(NULL);
    user->current_dir_id = 0;
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
            int ret = selectUserInfo(user, response);
            if (0 == ret) {
                log_info("Username '%s' found, sending salt:'%s'", task->data, response);
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
                userSuccessLoginInit(user);
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
 * @brief 用户注册校验用户名
 * @param task 对应的线程
 */
void userRegisterVerifyUsername(task_t *task) {
    char response[RESPONSE_BUFF_SIZE];
    int responseLen = 0;
    ResponseStatus status;
    int ret = selectUsernameUnique(task->data);
    if (ret == 0) {
        responseLen = snprintf(response, sizeof(response), "username is valid.\n");
        status = STATUS_SUCCESS;
    } else {
        status = STATUS_INVALID_PARAM;
        responseLen = snprintf(response, sizeof(response), "username is invalid.\n");
    }
    sendResponse(task->peerFd, status, DATA_TYPE_TEXT, response, responseLen);
}
/**
 * @brief 用户注册校验密码
 * @param task 对应的线程
 */
void userRegisterVerifyPassword(task_t *task) {
    char response[RESPONSE_BUFF_SIZE];
    int responseLen = 0;
    ResponseStatus status = STATUS_SUCCESS;
    char username[USERNAME_LENGTH];
    char password[PASSWORD_LENGTH];
    if (sscanf(task->data, "%s\n%s", username, password) < 0) {
        status = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "register failed,parse error");
    }
    log_info("register username=%s,password=%s", username, password);
    int userId = insertUser(username, password);
    if (userId < 0) {
        status = STATUS_FAIL;
        responseLen =
            snprintf(response, sizeof(response), "register failed,insert into table error.");
    } else {
        int ret = initUserVirtualTable(userId);
        if (ret < 0) {
            responseLen =
                snprintf(response, sizeof(response), "register successfully,but initilize failed.");
        } else {
            responseLen =
                snprintf(response, sizeof(response), "register successfully,please login");
        }
    }
    sendResponse(task->peerFd, status, DATA_TYPE_TEXT, response, responseLen);
}
/**
 * @brief 列出目录内容命令(数据库版本)
 */
void lsCommandVitrual(task_t *task) {
    log_info("Executing ls command (virtual, fd=%d)", task->peerFd);

    // 1. 获取用户信息
    user_info_t *user = getListUser(task->peerFd);
    if (NULL == user) {
        log_error("User not logged in (fd=%d)", task->peerFd);
        const char *errorMsg = "Error: Please login first\n";
        sendResponse(task->peerFd, STATUS_FAIL, DATA_TYPE_TEXT, errorMsg, strlen(errorMsg));
        return;
    }
    ResponseStatus statusCode = STATUS_SUCCESS;
    char response[RESPONSE_LENGTH] = {0};
    size_t offset = 0;
    int count = 0;
    // 2. 从数据库获取文件列表
    file_t files[FILE_MAX_CNT];
    int fileCount = listFiles(user, files, FILE_MAX_CNT);
    if (fileCount < 0) {
        log_error("Failed to list files from database (fd=%d)", task->peerFd);
        statusCode = STATUS_FAIL;
        offset = snprintf(response, sizeof(response), "ls error: database query failed\n");
        goto send_response;
    }
    log_info("Retrieved %d files from database", fileCount);
    // 3. 添加表头
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
    // 4. 遍历文件列表并格式化输出
    for (int i = 0; i < fileCount; i++) {
        file_t *file = &files[i];
        // 确定文件类型
        const char *typeStr;
        char fileName[FILE_NAME_MAX_LEN + 2]; // +2 for potential '/' suffix
        if (file->type == 2) {                // 假设 0 表示目录
            typeStr = "<DIR>";
            snprintf(fileName, sizeof(fileName), "%s/", file->filename);
        } else { // 1 表示普通文件
            typeStr = "FILE";
            strncpy(fileName, file->filename, sizeof(fileName) - 1);
            fileName[sizeof(fileName) - 1] = '\0';
        }
        // 格式化文件大小
        char sizeStr[13];
        if (file->type == 0) { // 目录
            snprintf(sizeStr, sizeof(sizeStr), "-");
        } else if (file->file_size < 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%lu B", (unsigned long)file->file_size);
        } else if (file->file_size < 1024 * 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", file->file_size / 1024.0);
        } else if (file->file_size < 1024ULL * 1024 * 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", file->file_size / (1024.0 * 1024.0));
        } else {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f GB",
                     file->file_size / (1024.0 * 1024.0 * 1024.0));
        }
        // 格式化更新时间
        char timeStr[17];
        struct tm *tm_info = localtime(&file->update_time);
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
    // 5. 添加汇总信息
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
 * @brief        : 当前工作目录命令
 * @param         {task_t} *task: 任务结构
**/
void pwdCommandVirtual(task_t *task) {
    log_info("Executing pwd command (fd=%d)", task->peerFd);
    ResponseStatus statusCode = STATUS_SUCCESS;
    char response[RESPONSE_LENGTH] = {0};
    size_t responseLen = 0;

    user_info_t *user = getListUser(task->peerFd);
    if (!user) {
        log_error("pwd: User not found for fd=%d", task->peerFd);
        const char *errorMsg = "Error: User session not found\n";
        sendResponse(task->peerFd, STATUS_FAIL, DATA_TYPE_TEXT, errorMsg, strlen(errorMsg));
        return;
    }

    log_debug("Current virtual path: %s", user->pwd);
    responseLen = snprintf(response, sizeof(response), "%s\n", user->pwd);

    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("pwd: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("pwd command completed: status=%d, path=%s", statusCode, user->pwd);
    }
}
/**
 * @brief 更改用户的工作目录
 * @param user 需要更改的用户
 * @param parsePath 解析后的目标路径
 * @param pathCount 路径段数量
 */
void changeUserPwd(user_info_t *user, char parsePath[][PATH_SEGMENT_MAX_LENGTH], int pathCount) {
    if (!user || !parsePath || pathCount <= 0) {
        return;
    }
    // 清空当前路径
    memset(user->pwd, 0, sizeof(user->pwd));
    // 重建路径字符串
    size_t offset = 0;
    // 第一个元素是根目录 "/"
    if (strcmp(parsePath[0], "/") == 0) {
        user->pwd[offset++] = '/';
    }
    // 拼接后续路径段
    for (int i = 1; i < pathCount && parsePath[i][0] != '\0' && offset < PATH_MAX_LENGTH - 1; i++) {
        // 只要不是刚开始就在根目录,就需要添加分隔符
        if (offset > 0 && user->pwd[offset - 1] != '/') {
            user->pwd[offset++] = '/';
        }
        // 拷贝路径段
        size_t len = strlen(parsePath[i]);
        if (offset + len >= PATH_MAX_LENGTH - 1) {
            log_warn("Path too long, truncating");
            break;
        }
        strcpy(user->pwd + offset, parsePath[i]);
        offset += len;
    }
    // 确保以 null 结尾
    user->pwd[offset] = '\0';
    // 特殊情况:如果只有根目录,确保是 "/"
    if (offset == 1 && user->pwd[0] == '/') {
        user->pwd[1] = '\0';
    }
    log_info("User pwd updated to: %s", user->pwd);
}
/**
 * @brief 进入某一目录的命令
 * @param task 执行该命令的线程
 */
void cdCommandVirtual(task_t *task) {
    log_info("Executing cd command (fd=%d)", task->peerFd);
    char response[RESPONSE_LENGTH];
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
    user_info_t *user = getListUser(task->peerFd);
    if (!user) {
        log_error("cd: User not found for fd=%d", task->peerFd);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "cd error: user session not found\n");
        goto send_response;
    }
    char parsePath[PATH_MAX_SEGMENTS][PATH_SEGMENT_MAX_LENGTH];
    // 检查路径解析是否成功
    int pathCount = parsePathToArray(task->data, parsePath, user->pwd);
    if (pathCount < 0) {
        log_error("cd: Path parsing failed (invalid path or too many '..'): %s", task->data);
        statusCode = STATUS_INVALID_PARAM;
        responseLen = snprintf(response, sizeof(response),
                               "cd error: invalid path (cannot go beyond root directory)\n");
        goto send_response;
    }
    int directoryId = getDirectoryId(user, parsePath, pathCount);
    if (directoryId < 0) {
        log_warn("cd: Directory not found: %s", task->data);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response),
                               "cd error: directory '%s' does not exist\n", task->data);
    } else {
        changeUserPwd(user, parsePath, pathCount);
        user->current_dir_id = directoryId; // 同步更新 current_dir_id
        log_info("cd: Changed to directory: %s (id=%d)", user->pwd, directoryId);
        responseLen = snprintf(response, sizeof(response), "Changed to directory: %s\n", user->pwd);
    }
send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("cd: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("cd command completed: status=%d, new_path=%s", statusCode,
                 statusCode == STATUS_SUCCESS ? user->pwd : "error");
    }
}
/**
 * @brief 进入某一目录的命令
 * @param task 执行该命令的线程
 */
void mkdirCommandVirtual(task_t *task) {
    log_info("Executing mkdir command (fd=%d)", task->peerFd);
    char response[RESPONSE_LENGTH];
    ResponseStatus statusCode = STATUS_SUCCESS;
    ssize_t responseLen = 0;
    user_info_t *user = getListUser(task->peerFd);
    // 1. 检查参数是否为空
    if (task->data[0] == '\0') {
        log_error("mkdir: Empty directory name");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "mkdir error: directory name cannot be empty\n");
        goto send_response;
    }
    char parsePath[PATH_MAX_SEGMENTS][PATH_SEGMENT_MAX_LENGTH];
    int pathCount = parsePathToArray(task->data, parsePath, user->pwd);
    int ret = resolveOrCreateDirectory(user, parsePath, pathCount);
    if (ret >= 0) {
        responseLen = snprintf(response, sizeof(response), "mkdir %s successfully.\n", task->data);
    } else {
        responseLen =
            snprintf(response, sizeof(response), "mkdir %s failed(unknown error).\n", task->data);
    }

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("mkdir: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("mkdir successfully.");
    }
}
/**
 * @brief 删除某一文件目录,如果该目录下有文件一并删除
 * @param task 执行该线程对应的线程
 */
void rmdirCommandVirtual(task_t *task) {
    log_info("Executing rmdir command (fd=%d)", task->peerFd);
    char response[RESPONSE_LENGTH];
    ResponseStatus statusCode = STATUS_SUCCESS;
    ssize_t responseLen = 0;
    user_info_t *user = getListUser(task->peerFd);
    // 1. 检查参数是否为空
    if (task->data[0] == '\0') {
        log_error("rmdir: Empty directory name");
        statusCode = STATUS_INVALID_PARAM;
        responseLen =
            snprintf(response, sizeof(response), "rmdir error: directory name cannot be empty\n");
        goto send_response;
    }
    char parsePath[PATH_MAX_SEGMENTS][PATH_SEGMENT_MAX_LENGTH];
    int pathCount = parsePathToArray(task->data, parsePath, user->pwd);
    int targetId = getDirectoryId(user, parsePath, pathCount);
    int ret = deleteFiles(user, targetId);
    if (ret < 0) {
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "rmdir error: unknown error\n");
        goto send_response;
    }
    responseLen = snprintf(response, sizeof(response), "rmdir successfully\n");
send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("rmdir: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("rmdir successfully.");
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
            // pwdCommand(task);
            pwdCommandVirtual(task);
            break;
        case CMD_TYPE_CD:
            // cdCommand(task);
            cdCommandVirtual(task);
            break;
        case CMD_TYPE_LS:
            // lsCommand(task);
            lsCommandVitrual(task);
            break;
        case CMD_TYPE_MKDIR:
            // mkdirCommand(task);
            mkdirCommandVirtual(task);
            break;
        case CMD_TYPE_RMDIR:
            rmdirCommandVirtual(task);
            // rmdirCommand(task);
            break;
        case CMD_TYPE_NOTCMD:
            notCommand(task);
            break;
        case CMD_TYPE_PUTS:
            // putsCommand(task);
            putsCommandVirtual(task);
            break;
        case CMD_TYPE_GETS:
            getsCommandVirtual(task);
            break;
        case CMD_TYPE_CHECK_PARTIAL:
            checkPartialFile(task);
            break;
        case TASK_REGISTER_USERNAME:
            userRegisterVerifyUsername(task);
            break;
        case TASK_REGISTER_PASSWORD:
            userRegisterVerifyPassword(task);
            break;
        case TASK_LOGIN_VERIFY_USERNAME:
            userLoginVerifyUsername(task);
            break;
        case TASK_LOGIN_VERIFY_PASSWORD:
            userLoginVerifyPassword(task);
            break;
    }
}