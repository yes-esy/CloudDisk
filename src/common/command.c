/**
 * @FilePath     : /CloudDisk/src/common/command.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-14 23:37:18
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
/**
 * @brief 发送响应给客户端
 */
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
 * @brief        : 当前工作目录命令
 * @param         {task_t} *task: 任务结构
 * @return        {*}
**/
void pwdCommand(task_t *task) {
    log_info("Executing pwd command (fd=%d)", task->peerFd);

    ResponseStatus statusCode = STATUS_SUCCESS;
    char realPath[PATH_MAX_LENGTH] = {0};
    char virtualPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_LENGTH] = {0};
    size_t responseLen = 0;

    // 获取当前真实工作目录
    if (getcwd(realPath, sizeof(realPath)) == NULL) {
        log_error("getcwd failed: %s", strerror(errno));
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "pwd error: %s\n", strerror(errno));
        goto send_response;
    }

    log_debug("Real path: %s", realPath);

    // 将真实路径转换为虚拟路径
    if (realPathToVirtual(realPath, virtualPath, sizeof(virtualPath)) != 0) {
        log_error("Path conversion failed: %s", realPath);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "pwd error: path conversion failed\n");
        goto send_response;
    }

    log_debug("Virtual path: %s", virtualPath);
    responseLen = snprintf(response, sizeof(response), "%s\n", virtualPath);

send_response:
    if (sendResponse(task->peerFd, statusCode, DATA_TYPE_TEXT, response, responseLen) < 0) {
        log_error("pwd: Failed to send response to client (fd=%d)", task->peerFd);
    } else {
        log_info("pwd command completed: status=%d, path=%s", statusCode,
                 statusCode == STATUS_SUCCESS ? virtualPath : "error");
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
    if (virtualPathToReal(VIRTUAL_ROOT, realPath, sizeof(realPath)) < 0) {
        log_error("Virtual path conversion failed: %s", VIRTUAL_ROOT);
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

void cdCommand(task_t *task) {
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
    if (virtualPathToReal(VIRTUAL_ROOT, currentRealPath, sizeof(currentRealPath)) < 0) {
        log_error("Virtual path conversion failed: %s", VIRTUAL_ROOT);
        statusCode = STATUS_FAIL;
        responseLen = snprintf(response, sizeof(response), "mkdir error: path conversion failed\n");
        goto send_response;
    }

    log_debug("Current real path: %s", currentRealPath);

    // 3. 获取目标目录的完整路径
    if (getDirectoryFullPath(task->data, currentRealPath, directoryFullPath) < 0) {
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

void rmdirCommand(task_t *task) {
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

void putsCommand(task_t *task) {
}

void getsCommand(task_t *task) {
}
void userLoginCheck1(task_t *task) {
}
void userLoginCheck2(task_t *task) {
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
        case TASK_LOGIN_SECTION1:
            userLoginCheck1(task);
            break;
        case TASK_LOGIN_SECTION1_RESP_OK:
            userLoginCheck1(task);
            break;
        case TASK_LOGIN_SECTION1_RESP_ERROR:
            userLoginCheck1(task);
            break;
        case TASK_LOGIN_SECTION2:
            userLoginCheck2(task);
            break;
        case TASK_LOGIN_SECTION2_RESP_OK:
            userLoginCheck2(task);
            break;
        case TASK_LOGIN_SECTION2_RESP_ERROR:
            userLoginCheck2(task);
            break;
    }
}