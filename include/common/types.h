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
#include <stdint.h>
/* 数据包缓冲区大小 */
#define PACKET_BUFF_SIZE 1000
#define TASK_DATA_SIZE 1000

/* 路径相关常量 */
#define PATH_MAX_LENGTH 4096
#define CLOUD_DISK_ROOT "/home/yes/Projects/CloudDisk/workforlder"
#define VIRTUAL_ROOT "/"
#define RESPONSE_LENGTH 4096
#define RESPONSE_BUFF_SIZE 4096

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

/**
 * 数据类型枚举
 */
typedef enum {
    DATA_TYPE_TEXT = 0,
    DATA_TYPE_BINARY = 1,
    DATA_TYPE_UNKOWN = 2,
} DataType;

/**
 * 响应状态码枚举
 */
typedef enum {
    STATUS_SUCCESS = 0,
    STATUS_FAIL = -1,
    STATUS_NOT_FOUND = -2,
    STATUS_PERMISSION_DENIED = -3,
    STATUS_INVALID_PARAM = -4,
} ResponseStatus;

/* 响应包头结构 (固定12字节) */
typedef struct {
    uint32_t dataLen;          /* 数据长度 */
    ResponseStatus statusCode; /* 状态码 (4字节) */
    DataType dataType;         /* 数据类型 (4字节) */
} __attribute__((packed)) response_header_t;

/* 完整响应包结构 (服务器 -> 客户端) */
typedef struct {
    response_header_t header;
    char data[RESPONSE_BUFF_SIZE];
} response_t;
/* 请求包结构 (客户端 -> 服务器) */
typedef struct {
    int len;                     /* 内容长度 */
    CmdType cmdType;             /* 命令类型 */
    char buff[PACKET_BUFF_SIZE]; /* 内容缓冲区 */
} packet_t;

/**
 * @brief 任务结构
 */
typedef struct task_t {
    int peerFd;                     /* 与client通信的套接字 */
    int epFd;                       /* epoll实例 */
    char data[TASK_DATA_SIZE];      /* 任务数据 */
    struct task_t *pNext;           /* 下一个任务 */
    CmdType type;                   /* 命令类型 */
    char currPath[PATH_MAX_LENGTH]; /*虚拟初始路径 */
} task_t;

#endif /* CLOUDDISK_TYPES_H */
