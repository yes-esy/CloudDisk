/**
 * @FilePath     : /CloudDisk/src/server/handler.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-14 23:08:27
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "handler.h"
#include "net.h"
#include <stdlib.h>
#include <unistd.h>
void handleMessage(int sockFd, int epFd, taskQueue_t *queue) {
    log_info(" handleMessage: Processing message from sockFd %d", sockFd);

    // 消息格式: cmd content
    //1.1 获取消息长度
    int length = -1;
    log_info(" handleMessage: Receiving message length...");
    int ret = recvn(sockFd, &length, sizeof(length));
    log_info(" handleMessage: recvn length returned %d, length value: %d", ret, length);

    if (0 == ret) {
        log_info(" handleMessage: Connection closed while receiving length");
        goto end;
    }

    //1.2 获取消息类型
    int cmdType = -1;
    log_info(" handleMessage: Receiving command type...");
    ret = recvn(sockFd, &cmdType, sizeof(cmdType));
    log_info(" handleMessage: recvn cmdType returned %d, cmdType value: %d", ret, cmdType);

    if (ret == 0) {
        log_info(" handleMessage: Connection closed while receiving cmdType");
        goto end;
    }

    log_info(" handleMessage: Creating task for sockFd %d, cmdType %d, length %d", sockFd,
           cmdType, length);

    task_t *task = (task_t *)calloc(1, sizeof(task_t));
    task->peerFd = sockFd;
    task->epFd = epFd;
    task->type = cmdType;

    if (length > 0) {
        log_info(" handleMessage: Receiving message content (%d bytes)...", length);
        //1.3 获取消息内容
        ret = recvn(sockFd, task->data, length);
        log_info(" handleMessage: recvn data returned %d", ret);

        if (ret > 0) {
            log_info(" handleMessage: Received data: %.*s", length, task->data);
            //往线程池中添加任务
            if (task->type == CMD_TYPE_PUTS) {
                log_info(" handleMessage: PUTS command detected, removing from epoll");
                //是上传文件任务，就暂时先从epoll中删除监听
                delEpollReadfd(epFd, sockFd);
            }
            log_info(" handleMessage: Enqueueing task to thread pool");
            taskEnque(queue, task);
        } else {
            log_info(" handleMessage: Failed to receive data, ret: %d", ret);
            free(task);
        }
    } else if (length == 0) {
        log_info(" handleMessage: Zero length message, enqueueing task");
        taskEnque(queue, task);
    } else {
        log_info(" handleMessage: Invalid length %d, freeing task", length);
        free(task);
    }

end:
    if (0 == ret) {
        log_info(" handleMessage: Connection %d is closed, cleaning up", sockFd);
        printf("conn %d is closed.", sockFd);
        delEpollReadfd(epFd, sockFd);
        close(sockFd);
    }
    log_info(" handleMessage: Finished processing message from sockFd %d", sockFd);
}