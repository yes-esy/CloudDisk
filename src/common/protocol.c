/**
 * @file protocol.c
 * @brief 协议解析实现
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#include "protocol.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

int getCommandType(const char *str) {
    if (strcmp(str, "pwd") == 0)
        return CMD_TYPE_PWD;
    if (strcmp(str, "ls") == 0)
        return CMD_TYPE_LS;
    if (strcmp(str, "cd") == 0)
        return CMD_TYPE_CD;
    if (strcmp(str, "mkdir") == 0)
        return CMD_TYPE_MKDIR;
    if (strcmp(str, "rmdir") == 0)
        return CMD_TYPE_RMDIR;
    if (strcmp(str, "puts") == 0)
        return CMD_TYPE_PUTS;
    if (strcmp(str, "gets") == 0)
        return CMD_TYPE_GETS;
    return CMD_TYPE_NOTCMD;
}

void splitString(const char *pstrs, const char *delimiter, char *tokens[], int max_tokens,
                 int *pcount) {
    if (!pstrs || !delimiter || !tokens || !pcount) {
        if (pcount)
            *pcount = 0;
        return;
    }

    char *str_copy = strdup(pstrs);
    if (!str_copy) {
        *pcount = 0;
        return;
    }

    char *token = strtok(str_copy, delimiter);
    int count = 0;
    while (token != NULL && count < max_tokens) {
        tokens[count++] = strdup(token);
        token = strtok(NULL, delimiter);
    }
    *pcount = count;
    free(str_copy);
}

/**
 * @brief 处理puts命令参数: "src/path/filename destpath" -> "filename destpath"
 * @param src 原始参数字符串
 * @param dest 输出缓冲区
 * @return 成功返回0,失败返回-1
 */
int processPutsCommand(const char *src, char *dest) {
    if (!src || !dest) {
        return -1;
    }

    int len = strlen(src);
    if (len == 0) {
        dest[0] = '\0';
        return -1;
    }

    // 1. 找到第一个空格位置(分隔源路径和目标路径)
    int spacePos = -1;
    for (int i = 0; i < len; i++) {
        if (isspace((unsigned char)src[i])) {
            spacePos = i;
            break;
        }
    }

    if (spacePos == -1) {
        // 没有目标路径,格式错误
        dest[0] = '\0';
        return -1;
    }

    // 2. 在源路径中找最后一个斜杠
    int lastSlashPos = -1;
    for (int i = spacePos - 1; i >= 0; i--) {
        if (src[i] == '/') {
            lastSlashPos = i;
            break;
        }
    }

    // 3. 提取文件名(从斜杠后或开头到空格前)
    int destIndex = 0;
    int filenameStart = (lastSlashPos == -1) ? 0 : lastSlashPos + 1;
    for (int i = filenameStart; i < spacePos; i++) {
        dest[destIndex++] = src[i];
    }

    // 4. 添加空格分隔符
    dest[destIndex++] = ' ';

    // 5. 跳过中间的空白字符
    int destPathStart = spacePos;
    while (destPathStart < len && isspace((unsigned char)src[destPathStart])) {
        destPathStart++;
    }

    // 6. 复制目标路径(去除尾部空白)
    int destPathEnd = len - 1;
    while (destPathEnd >= destPathStart && isspace((unsigned char)src[destPathEnd])) {
        destPathEnd--;
    }

    for (int i = destPathStart; i <= destPathEnd; i++) {
        dest[destIndex++] = src[i];
    }

    dest[destIndex] = '\0';
    return 0;
}
/**
 * @brief 处理puts命令参数: "src/path/filename destpath" -> "filepath/filename"
 * @param src 原始参数字符串
 * @param dest 输出缓冲区
 * @return 成功返回0,失败返回-1
 */
int processGetsCommand(const char *src, char *dest) {
    if (!src || !dest) {
        return -1;
    }

    int len = strlen(src);
    if (len == 0) {
        dest[0] = '\0';
        return -1;
    }

    // 去除尾部空白字符
    int end = len - 1;
    while (end >= 0 && isspace((unsigned char)src[end])) {
        end--;
    }

    // 复制有效部分
    int destIndex = 0;
    for (int i = 0; i <= end; i++) {
        if (isspace((unsigned char)src[i])) {
            // 中间遇到空白字符,停止复制
            break;
        }
        dest[destIndex++] = src[i];
    }
    dest[destIndex] = '\0';
    return 0;
}
/**
 * @brief 解析命令
 * @param pinput 输入的内容
 * @param len 长度
 * @param pt 数据包
 * @param processedArgs 输出原始参数(可选)
 * @return 成功返回0,失败返回-1
 */
int parseCommand(const char *pinput, int len, packet_t *pt, char *processedArgs) {
    if (!pinput || !pt || len <= 0) {
        return -1;
    }

    /* 去除前后空白字符 */
    while (len > 0 && isspace((unsigned char)pinput[len - 1])) {
        len--;
    }
    while (len > 0 && isspace((unsigned char)*pinput)) {
        pinput++;
        len--;
    }

    if (len == 0) {
        pt->cmdType = CMD_TYPE_NOTCMD;
        pt->len = 0;
        pt->buff[0] = '\0';
        return 0;
    }

    /* 提取命令 */
    char cmd[32] = {0};
    int i = 0;
    while (i < len && !isspace((unsigned char)pinput[i]) && i < 31) {
        cmd[i] = pinput[i];
        i++;
    }
    cmd[i] = '\0';

    pt->cmdType = getCommandType(cmd);

    /* 跳过命令后的空白 */
    while (i < len && isspace((unsigned char)pinput[i])) {
        i++;
    }

    /* 复制剩余参数 */
    int argLen = len - i;
    if (argLen > 0) {
        // 限制参数长度
        if (argLen >= PACKET_BUFF_SIZE) {
            argLen = PACKET_BUFF_SIZE - 1;
        }

        // 如果需要,保存原始参数
        if (processedArgs) {
            memcpy(processedArgs, pinput + i, argLen);
            processedArgs[argLen] = '\0';
        }

        char extractedArgs[ARGS_LENGTH] = {0};
        int extractArgsLen = 0;
        // 处理不同命令类型
        if (pt->cmdType == CMD_TYPE_PUTS) {
            // puts命令处理
            // pt->buff 存放 "filename destpath"
            if (processPutsCommand(pinput + i, extractedArgs) != 0) {
                // 处理失败,返回空
                goto process_fail;
            }
        } else if (CMD_TYPE_GETS == pt->cmdType) {
            // gets命令处理
            // pt->buff 存放 "path/filename"
            if (processGetsCommand(pinput + i, extractedArgs) != 0) {
                // 处理失败,返回空
                goto process_fail;
            }
        }
        if (strlen(extractedArgs) > 0) {
            extractArgsLen = strlen(extractedArgs);
            memcpy(pt->buff, extractedArgs, extractArgsLen);
            pt->buff[extractArgsLen] = '\0';
            pt->len = extractArgsLen;
        } else {
            // 其他命令直接复制参数
            memcpy(pt->buff, pinput + i, argLen);
            pt->buff[argLen] = '\0';
            pt->len = argLen;
        }
    }
    return 0;
process_fail:
    pt->buff[0] = '\0';
    pt->len = 0;
    return -1;
}
