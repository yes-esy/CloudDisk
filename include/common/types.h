/**
 * @file types.h
 * @brief 基础类型定义
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_TYPES_H
#define CLOUDDISK_TYPES_H

#include <stddef.h>
#include <sys/types.h>

/* 数据包缓冲区大小 */
#define PACKET_BUFF_SIZE 1000
#define TASK_DATA_SIZE 1000

/* 路径相关常量 */
#define PATH_MAX_LENGTH 2048
#define CLOUD_DISK_ROOT "/home/yes/Projects/CloudDisk/CloudDiskWorkForlder"
#define VIRTUAL_ROOT "/"
#define RESPONSE_LENGTH 4096

/* 命令类型枚举 */
typedef enum {
    CMD_TYPE_PWD = 1,
    CMD_TYPE_LS,
    CMD_TYPE_CD,
    CMD_TYPE_MKDIR,
    CMD_TYPE_RMDIR,
    CMD_TYPE_PUTS,
    CMD_TYPE_GETS,
    CMD_TYPE_NOTCMD,

    TASK_LOGIN_SECTION1 = 100,
    TASK_LOGIN_SECTION1_RESP_OK,
    TASK_LOGIN_SECTION1_RESP_ERROR,
    TASK_LOGIN_SECTION2,
    TASK_LOGIN_SECTION2_RESP_OK,
    TASK_LOGIN_SECTION2_RESP_ERROR,
} CmdType;

/* 数据包结构 */
typedef struct {
    int len;                      /* 内容长度 */
    CmdType cmdType;              /* 命令类型 */
    char buff[PACKET_BUFF_SIZE];  /* 内容缓冲区 */
} packet_t;

#endif /* CLOUDDISK_TYPES_H */
