/**
 * @FilePath     : /CloudDisk/src/server/Communication.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-11 23:01:39
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Communication.h"
#include "Net.h"

void handleMessage(int sockFd, int epFd, taskQueue_t *queue) {
    printf("[DEBUG] handleMessage: Processing message from sockFd %d\n", sockFd);

    // 消息格式: cmd content
    //1.1 获取消息长度
    int length = -1;
    printf("[DEBUG] handleMessage: Receiving message length...\n");
    int ret = recvn(sockFd, &length, sizeof(length));
    printf("[DEBUG] handleMessage: recvn length returned %d, length value: %d\n", ret, length);

    if (0 == ret) {
        printf("[DEBUG] handleMessage: Connection closed while receiving length\n");
        goto end;
    }

    //1.2 获取消息类型
    int cmdType = -1;
    printf("[DEBUG] handleMessage: Receiving command type...\n");
    ret = recvn(sockFd, &cmdType, sizeof(cmdType));
    printf("[DEBUG] handleMessage: recvn cmdType returned %d, cmdType value: %d\n", ret, cmdType);

    if (ret == 0) {
        printf("[DEBUG] handleMessage: Connection closed while receiving cmdType\n");
        goto end;
    }

    printf("[DEBUG] handleMessage: Creating task for sockFd %d, cmdType %d, length %d\n", sockFd,
           cmdType, length);

    task_t *task = (task_t *)calloc(1, sizeof(task_t));
    task->peerFd = sockFd;
    task->epFd = epFd;
    task->type = cmdType;

    if (length > 0) {
        printf("[DEBUG] handleMessage: Receiving message content (%d bytes)...\n", length);
        //1.3 获取消息内容
        ret = recvn(sockFd, task->data, length);
        printf("[DEBUG] handleMessage: recvn data returned %d\n", ret);

        if (ret > 0) {
            printf("[DEBUG] handleMessage: Received data: %.*s\n", length, task->data);
            //往线程池中添加任务
            if (task->type == CMD_TYPE_PUTS) {
                printf("[DEBUG] handleMessage: PUTS command detected, removing from epoll\n");
                //是上传文件任务，就暂时先从epoll中删除监听
                delEpollReadfd(epFd, sockFd);
            }
            printf("[DEBUG] handleMessage: Enqueueing task to thread pool\n");
            taskEnque(queue, task);
        } else {
            printf("[DEBUG] handleMessage: Failed to receive data, ret: %d\n", ret);
            free(task);
        }
    } else if (length == 0) {
        printf("[DEBUG] handleMessage: Zero length message, enqueueing task\n");
        taskEnque(queue, task);
    } else {
        printf("[DEBUG] handleMessage: Invalid length %d, freeing task\n", length);
        free(task);
    }

end:
    if (0 == ret) {
        printf("[DEBUG] handleMessage: Connection %d is closed, cleaning up\n", sockFd);
        printf("\nconn %d is closed.\n", sockFd);
        delEpollReadfd(epFd, sockFd);
        close(sockFd);
    }
    printf("[DEBUG] handleMessage: Finished processing message from sockFd %d\n", sockFd);
}