/**
 * @file handler.h
 * @brief 服务器请求处理器
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_HANDLER_H
#define CLOUDDISK_HANDLER_H

#include "macros.h"
#include "threadpool.h"

/**
 * @brief 主线程调用:处理客户端发过来的消息
 * @param sockFd 客户端socket
 * @param epFd epoll文件描述符
 * @param queue 任务队列指针
 */
void handleMessage(int sockFd, int epFd, taskQueue_t *queue);

#endif /* CLOUDDISK_HANDLER_H */
