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

void splitString(const char *pstrs, const char *delimiter, char *tokens[], 
                 int max_tokens, int *pcount) {
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
 * @brief        : 解析命令
 * @param         {char} *input: 输入的内容
 * @param         {int} len: 长度
 * @param         {packet_t} *pt: 数据包
 * @return        {int} : 返回0
**/
int parseCommand(const char *pinput, int len, packet_t *pt) {
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
        if (argLen >= PACKET_BUFF_SIZE) {
            argLen = PACKET_BUFF_SIZE - 1;
        }
        memcpy(pt->buff, pinput + i, argLen);
        pt->buff[argLen] = '\0';
        pt->len = argLen;
    } else {
        pt->buff[0] = '\0';
        pt->len = 0;
    }

    return 0;
}
