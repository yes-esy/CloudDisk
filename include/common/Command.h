/**
 * @FilePath     : /CloudDisk/include/common/Command.h
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-11 23:28:47
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#define PATH_MAX_LENGTH 2048
// 定义云盘的根目录和虚拟根路径
#define CLOUD_DISK_ROOT "/home/yes/Projects/CloudDisk/CloudDiskWorkForlder"
#define VIRTUAL_ROOT "/"
#define RESPONSE_LENGTH 4096
typedef struct task_t task_t;
/**
 * 命令
 */
typedef enum {
    CMD_TYPE_PWD = 1,
    CMD_TYPE_LS,
    CMD_TYPE_CD,
    CMD_TYPE_MKDIR,
    CMD_TYPE_RMDIR,
    CMD_TYPE_PUTS,
    CMD_TYPE_GETS,
    CMD_TYPE_NOTCMD, //不是命令

    TASK_LOGIN_SECTION1 = 100,
    TASK_LOGIN_SECTION1_RESP_OK,
    TASK_LOGIN_SECTION1_RESP_ERROR,
    TASK_LOGIN_SECTION2,
    TASK_LOGIN_SECTION2_RESP_OK,
    TASK_LOGIN_SECTION2_RESP_ERROR,
} CmdType;
void pwdCommand(task_t *task);
void cdCommand(task_t *task);
void cdCommand(task_t *task);
void mkdirCommand(task_t *task);
void rmdirCommand(task_t *task);
void notCommand(task_t *task);
void putsCommand(task_t *task);
void getsCommand(task_t *task);
void userLoginCheck1(task_t *task);
void userLoginCheck2(task_t *task);
int getCommandType(const char *str);
void splitString(const char *pstrs, const char *delimiter, char *tokens[], int max_tokens,
                 int *pcount);
typedef struct packet_t packet_t;
//解析命令
int parseCommand(const char *pinput, int len, packet_t *pt);
void executeCmd(task_t *task);