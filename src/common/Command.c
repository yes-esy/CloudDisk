/**
 * @FilePath     : /CloudDisk/src/common/Command.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 23:16:20
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Command.h"
#include "Net.h"
#include "ThreadPool.h"
/**
 * @brief        : 当前工作目录
 * @param         {task_t} *task:
 * @return        {*}
**/
void pwdCommand(task_t *task) {
    char buf[PATH_MAX_LENGTH] = {0};
    if (getcwd(buf, sizeof(buf)) == NULL) {
        snprintf(buf, sizeof(buf), "pwd error: %s\n", strerror(errno));
    } else {
        int len = strlen(buf);
        if (len + 1 < sizeof(buf)) {
            buf[len] = '\n', buf[len + 1] = '\0';
        }
    }
    ssize_t ret = sendn(task->peerFd, buf, strlen(buf));
}
void cdCommand(task_t *task) {
}
void mkdirCommand(task_t *task) {
}
void rmdirCommand(task_t *task) {
}
void notCommand(task_t *task) {
}
void putsCommand(task_t *task) {
}
void getsCommand(task_t *task) {
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
            cdCommand(task);
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