/**
 * @FilePath     : /CloudDisk/include/common/Common.h
 * @Description  :  公共头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-09 23:06:24
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#pragma once
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "Command.h"
#define ARGS_CHECK(argc, num)                                                                      \
    {                                                                                              \
        if (argc != num) {                                                                         \
            fprintf(stderr, "ARGS ERROR!\n");                                                      \
            return -1;                                                                             \
        }                                                                                          \
    }

#define ERROR_CHECK(ret, num, msg)                                                                 \
    {                                                                                              \
        if (ret == num) {                                                                          \
            perror(msg);                                                                           \
            return -1;                                                                             \
        }                                                                                          \
    }
#define PACKET_BUFF_SIZE 1000
typedef struct {
    int len;         // 内容长度
    CmdType cmdType; // 命令类型
    char buff[PACKET_BUFF_SIZE]; // 记录内容本身
} packet_t;