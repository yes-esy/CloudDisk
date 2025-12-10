/**
 * @FilePath     : /CloudDisk/include/server/Communication.h
 * @Description  :  业务逻辑
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 20:49:19
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include "Common.h"
#include "ThreadPool.h"

//主线程调用:处理客户端发过来的消息
void handleMessage(int sockFd, int epFd, taskQueue_t *queue);