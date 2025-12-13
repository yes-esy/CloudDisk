/**
 * @file macros.h
 * @brief 公共宏定义
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_MACROS_H
#define CLOUDDISK_MACROS_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* 参数检查宏 */
#define ARGS_CHECK(argc, num)                                     \
    do {                                                          \
        if (argc != num) {                                        \
            fprintf(stderr, "ARGS ERROR!\n");                     \
            return -1;                                            \
        }                                                         \
    } while (0)

/* 错误检查宏 */
#define ERROR_CHECK(ret, num, msg)                                \
    do {                                                          \
        if (ret == num) {                                         \
            perror(msg);                                          \
            return -1;                                            \
        }                                                         \
    } while (0)

/* 线程错误检查宏 */
#define THREAD_ERROR_CHECK(ret, funcName)                         \
    do {                                                          \
        if (ret != 0) {                                           \
            fprintf(stderr, "%s:%s\n", funcName, strerror(ret));  \
        }                                                         \
    } while (0)

/* Epoll相关常量 */
#define EPOLL_ARR_SIZE 100

#endif /* CLOUDDISK_MACROS_H */
