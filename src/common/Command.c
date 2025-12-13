/**
 * @FilePath     : /CloudDisk/src/common/Command.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-12 23:17:25
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Command.h"
#include "Net.h"
#include "ThreadPool.h"
#include <dirent.h>
// #include <fcntl.h>
#include <sys/stat.h>
/**
 * @brief        : 将真实路径转换为虚拟路径
 * @param         {char} *realPath: 真实路径
 * @param         {char} *virtualPath: 输出的虚拟路径
 * @param         {size_t} virtualPathSize: 虚拟路径缓冲区大小
 * @return        {int} : 成功返回0，失败返回-1
**/
int realPathToVirtual(const char *realPath, char *virtualPath, size_t virtualPathSize) {
    if (!realPath || !virtualPath || virtualPathSize == 0) {
        return -1;
    }

    // 检查真实路径是否在云盘根目录下
    size_t rootLen = strlen(CLOUD_DISK_ROOT);
    if (strncmp(realPath, CLOUD_DISK_ROOT, rootLen) != 0) {
        // 不在云盘根目录下，返回虚拟根目录
        snprintf(virtualPath, virtualPathSize, "%s", VIRTUAL_ROOT);
        return 0;
    }

    // 获取相对于云盘根目录的路径部分
    const char *relativePath = realPath + rootLen;

    if (strlen(relativePath) == 0) {
        // 如果是云盘根目录，返回虚拟根目录
        snprintf(virtualPath, virtualPathSize, "%s", VIRTUAL_ROOT);
    } else {
        // 构造虚拟路径
        if (relativePath[0] == '/') {
            snprintf(virtualPath, virtualPathSize, "%s", relativePath);
        } else {
            snprintf(virtualPath, virtualPathSize, "/%s", relativePath);
        }
    }

    return 0;
}
/**
 * @brief        : 将虚拟路径转换为真实路径
 * @param         {char} *virtualPath: 虚拟路径
 * @param         {char} *realPath: 输出的真实路径
 * @param         {size_t} realPathSize: 真实路径缓冲区大小
 * @return        {int} : 成功返回0，失败返回-1
**/
int virtualPathToReal(const char *virtualPath, char *realPath, size_t realPathSize) {
    if (!virtualPath || !realPath || realPathSize == 0) {
        return -1;
    }

    // 如果虚拟路径是根目录
    if (strcmp(virtualPath, VIRTUAL_ROOT) == 0) {
        snprintf(realPath, realPathSize, "%s", CLOUD_DISK_ROOT);
        return 0;
    }

    // 构造真实路径
    if (virtualPath[0] == '/') {
        snprintf(realPath, realPathSize, "%s%s", CLOUD_DISK_ROOT, virtualPath);
    } else {
        snprintf(realPath, realPathSize, "%s/%s", CLOUD_DISK_ROOT, virtualPath);
    }

    return 0;
}

/**
 * @brief        : 当前工作目录命令
 * @param         {task_t} *task: 任务结构
 * @return        {*}
**/
void pwdCommand(task_t *task) {
    printf("[DEBUG] pwdCommand: Executing pwd command\n");

    char realPath[PATH_MAX_LENGTH] = {0};
    char virtualPath[PATH_MAX_LENGTH] = {0};
    char response[RESPONSE_LENGTH] = {0};
    // 获取当前真实工作目录
    if (getcwd(realPath, sizeof(realPath)) == NULL) {
        printf("[DEBUG] pwdCommand: getcwd failed: %s\n", strerror(errno));
        snprintf(response, sizeof(response) - 1, "pwd error: %s\n", strerror(errno));
    } else {
        printf("[DEBUG] pwdCommand: Real current directory: %s\n", realPath);

        // 将真实路径转换为虚拟路径
        if (realPathToVirtual(realPath, virtualPath, sizeof(virtualPath)) == 0) {
            printf("[DEBUG] pwdCommand: Virtual path: %s\n", virtualPath);
            snprintf(response, sizeof(response) - 1, "%s\n", virtualPath);
        } else {
            printf("[DEBUG] pwdCommand: Path conversion failed\n");
            snprintf(response, sizeof(response) - 1, "pwd error: path conversion failed\n");
        }
    }

    printf("[DEBUG] pwdCommand: Sending response: %s", response);
    ssize_t ret = sendn(task->peerFd, response, strlen(response));
    printf("[DEBUG] pwdCommand: sendn returned %zd\n", ret);
}
void cdCommand(task_t *task) {
    printf("[DEBUG] cdCommand: Executing cd command\n");

    char response[RESPONSE_LENGTH] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};
    char virtualPath[PATH_MAX_LENGTH] = {0};

    // 如果没有参数，切换到云盘根目录
    if (strlen(task->data) == 0) {
        strcpy(virtualPath, VIRTUAL_ROOT);
    } else {
        strncpy(virtualPath, task->data, sizeof(virtualPath) - 1);
    }

    printf("[DEBUG] cdCommand: Target virtual path: %s\n", virtualPath);

    // 将虚拟路径转换为真实路径
    if (virtualPathToReal(virtualPath, realPath, sizeof(realPath)) != 0) {
        snprintf(response, sizeof(response), "cd error: invalid path\n");
    } else {
        printf("[DEBUG] cdCommand: Target real path: %s\n", realPath);

        // 检查目录是否存在
        struct stat st;
        if (stat(realPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            // 切换到目标目录
            if (chdir(realPath) == 0) {
                snprintf(response, sizeof(response), "Directory changed to %s\n", virtualPath);
                printf("[DEBUG] cdCommand: Successfully changed to %s\n", realPath);
            } else {
                snprintf(response, sizeof(response), "cd error: %s\n", strerror(errno));
                printf("[DEBUG] cdCommand: chdir failed: %s\n", strerror(errno));
            }
        } else {
            snprintf(response, sizeof(response), "cd error: directory not found\n");
            printf("[DEBUG] cdCommand: Directory not found: %s\n", realPath);
        }
    }

    printf("[DEBUG] cdCommand: Sending response: %s", response);
    sendn(task->peerFd, response, strlen(response));
}

void lsCommand(task_t *task) {
    printf("[DEBUG] lsCommand: Executing ls command\n");

    char response[RESPONSE_LENGTH] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};
    char currentPath[PATH_MAX_LENGTH] = {0};

    // 获取当前工作目录
    if (getcwd(currentPath, sizeof(currentPath)) == NULL) {
        snprintf(response, sizeof(response), "ls error: %s\n", strerror(errno));
        goto send_response;
    }

    // 如果有参数，列出指定目录；否则列出当前目录
    if (strlen(task->data) > 0) {
        if (virtualPathToReal(task->data, realPath, sizeof(realPath)) != 0) {
            snprintf(response, sizeof(response), "ls error: invalid path\n");
            goto send_response;
        }
    } else {
        strncpy(realPath, currentPath, sizeof(realPath) - 1);
        realPath[sizeof(realPath) - 1] = '\0'; // 确保字符串终止
    }

    printf("[DEBUG] lsCommand: Listing directory: %s\n", realPath);

    DIR *dir = opendir(realPath);
    if (dir == NULL) {
        snprintf(response, sizeof(response), "ls error: %s\n", strerror(errno));
        goto send_response;
    }

    struct dirent *entry;
    char tempLine[512];
    int responseLen = 0;

    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 .. 目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullPath[PATH_MAX_LENGTH * 2];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", realPath, entry->d_name);

        struct stat st;
        if (stat(fullPath, &st) == 0) {
            char typeChar = S_ISDIR(st.st_mode) ? 'd' : '-';
            snprintf(tempLine, sizeof(tempLine), "%c %10ld %s\n", typeChar, st.st_size,
                     entry->d_name);
        } else {
            snprintf(tempLine, sizeof(tempLine), "? %10s %s\n", "unknown", entry->d_name);
        }

        // 检查是否会超出响应缓冲区
        if (responseLen + strlen(tempLine) >= sizeof(response) - 1) {
            break;
        }

        strcat(response, tempLine);
        responseLen += strlen(tempLine);
    }

    closedir(dir);

    if (responseLen == 0) {
        strcpy(response, "Directory is empty\n");
    }

send_response:
    printf("[DEBUG] lsCommand: Sending response (%zu bytes)\n", strlen(response));
    sendn(task->peerFd, response, strlen(response));
}

void mkdirCommand(task_t *task) {
    printf("[DEBUG] mkdirCommand: Executing mkdir command\n");

    char response[RESPONSE_LENGTH] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};

    if (strlen(task->data) == 0) {
        snprintf(response, sizeof(response), "mkdir error: missing directory name\n");
        goto send_response;
    }

    printf("[DEBUG] mkdirCommand: Creating directory: %s\n", task->data);

    // 将虚拟路径转换为真实路径
    if (virtualPathToReal(task->data, realPath, sizeof(realPath)) != 0) {
        snprintf(response, sizeof(response), "mkdir error: invalid path\n");
        goto send_response;
    }

    printf("[DEBUG] mkdirCommand: Real path: %s\n", realPath);

    // 创建目录
    if (mkdir(realPath, 0755) == 0) {
        snprintf(response, sizeof(response), "Directory '%s' created successfully\n", task->data);
        printf("[DEBUG] mkdirCommand: Directory created successfully\n");
    } else {
        snprintf(response, sizeof(response), "mkdir error: %s\n", strerror(errno));
        printf("[DEBUG] mkdirCommand: mkdir failed: %s\n", strerror(errno));
    }

send_response:
    printf("[DEBUG] mkdirCommand: Sending response: %s", response);
    sendn(task->peerFd, response, strlen(response));
}

void rmdirCommand(task_t *task) {
    printf("[DEBUG] rmdirCommand: Executing rmdir command\n");

    char response[RESPONSE_LENGTH] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};

    if (strlen(task->data) == 0) {
        snprintf(response, sizeof(response), "rmdir error: missing directory name\n");
        goto send_response;
    }

    printf("[DEBUG] rmdirCommand: Removing directory: %s\n", task->data);

    // 将虚拟路径转换为真实路径
    if (virtualPathToReal(task->data, realPath, sizeof(realPath)) != 0) {
        snprintf(response, sizeof(response), "rmdir error: invalid path\n");
        goto send_response;
    }

    printf("[DEBUG] rmdirCommand: Real path: %s\n", realPath);

    // 检查是否是云盘根目录
    if (strcmp(realPath, CLOUD_DISK_ROOT) == 0) {
        snprintf(response, sizeof(response), "rmdir error: cannot remove root directory\n");
        goto send_response;
    }

    // 删除目录
    if (rmdir(realPath) == 0) {
        snprintf(response, sizeof(response), "Directory '%s' removed successfully\n", task->data);
        printf("[DEBUG] rmdirCommand: Directory removed successfully\n");
    } else {
        snprintf(response, sizeof(response), "rmdir error: %s\n", strerror(errno));
        printf("[DEBUG] rmdirCommand: rmdir failed: %s\n", strerror(errno));
    }

send_response:
    printf("[DEBUG] rmdirCommand: Sending response: %s", response);
    sendn(task->peerFd, response, strlen(response));
}

void notCommand(task_t *task) {
    printf("[DEBUG] notCommand: Unknown command\n");
    char response[] = "Error: Unknown command\n";
    sendn(task->peerFd, response, strlen(response));
}

void putsCommand(task_t *task) {
    printf("[DEBUG] putsCommand: Executing file upload command\n");

    char response[512] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};

    if (strlen(task->data) == 0) {
        snprintf(response, sizeof(response), "puts error: missing filename\n");
        goto send_response;
    }

    printf("[DEBUG] putsCommand: Uploading file: %s\n", task->data);

    // 将虚拟路径转换为真实路径
    if (virtualPathToReal(task->data, realPath, sizeof(realPath)) != 0) {
        snprintf(response, sizeof(response), "puts error: invalid path\n");
        goto send_response;
    }

    printf("[DEBUG] putsCommand: Real path: %s\n", realPath);

    // TODO: 实现文件上传逻辑
    // 1. 接收文件大小
    // 2. 创建文件
    // 3. 接收文件数据
    // 4. 写入文件

    snprintf(response, sizeof(response), "puts command not fully implemented yet\n");

send_response:
    printf("[DEBUG] putsCommand: Sending response: %s", response);
    sendn(task->peerFd, response, strlen(response));
}

void getsCommand(task_t *task) {
    printf("[DEBUG] getsCommand: Executing file download command\n");

    char response[512] = {0};
    char realPath[PATH_MAX_LENGTH] = {0};

    if (strlen(task->data) == 0) {
        snprintf(response, sizeof(response), "gets error: missing filename\n");
        goto send_response;
    }

    printf("[DEBUG] getsCommand: Downloading file: %s\n", task->data);

    // 将虚拟路径转换为真实路径
    if (virtualPathToReal(task->data, realPath, sizeof(realPath)) != 0) {
        snprintf(response, sizeof(response), "gets error: invalid path\n");
        goto send_response;
    }

    printf("[DEBUG] getsCommand: Real path: %s\n", realPath);

    // 检查文件是否存在
    struct stat st;
    if (stat(realPath, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(response, sizeof(response), "gets error: file not found or not a regular file\n");
        goto send_response;
    }

    // TODO: 实现文件下载逻辑
    // 1. 发送文件大小
    // 2. 打开文件
    // 3. 发送文件数据

    snprintf(response, sizeof(response), "gets command not fully implemented yet\n");

send_response:
    printf("[DEBUG] getsCommand: Sending response: %s", response);
    sendn(task->peerFd, response, strlen(response));
}
void userLoginCheck1(task_t *task) {
}
void userLoginCheck2(task_t *task) {
}
int getCommandType(const char *str) {
    if (!strcmp(str, "pwd"))
        return CMD_TYPE_PWD;
    else if (!strcmp(str, "ls"))
        return CMD_TYPE_LS;
    else if (!strcmp(str, "cd"))
        return CMD_TYPE_CD;
    else if (!strcmp(str, "mkdir"))
        return CMD_TYPE_MKDIR;
    else if (!strcmp(str, "rmdir"))
        return CMD_TYPE_RMDIR;
    else if (!strcmp(str, "puts"))
        return CMD_TYPE_PUTS;
    else if (!strcmp(str, "gets"))
        return CMD_TYPE_GETS;
    else
        return CMD_TYPE_NOTCMD;
}
/**
 * @brief        : 分割字符串
 * @return        {*}
**/
void splitString(const char *pstrs, const char *delimiter, char *tokens[], int max_tokens,
                 int *pcount) {
    int token_count = 0;
    char *token = strtok((char *)pstrs, delimiter); // 使用delimiter作为分隔符

    while (token != NULL && token_count < max_tokens - 1) { // 保留一个位置给NULL终止符
        char *pstr = (char *)calloc(1, strlen(token) + 1);
        strcpy(pstr, token);
        tokens[token_count] = pstr; //保存申请的堆空间首地址
        token_count++;
        token = strtok(NULL, delimiter); // 继续获取下一个token
    }
    // 添加NULL终止符
    tokens[token_count] = NULL;
    *pcount = token_count;
}
/**
 * @brief        : 解析命令
 * @param         {char} *input: 输入的内容
 * @param         {int} len: 长度
 * @param         {packet_t} *pt: 数据包
 * @return        {int} : 返回0
**/
int parseCommand(const char *input, int len, packet_t *pt) {
    char *pstrs[10] = {0};
    int cnt = 0;
    splitString(input, " ", pstrs, 10, &cnt);
    pt->cmdType = getCommandType(pstrs[0]);
    //暂时限定命令行格式为：
    //1. cmd
    //2. cmd content
    if (cnt > 1) {
        pt->len = strlen(pstrs[1]);
        strncpy(pt->buff, pstrs[1], pt->len);
    } else {
        // 当只有命令没有参数时，长度为0
        pt->len = 0;
        pt->buff[0] = '\0';
    }
    return 0;
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