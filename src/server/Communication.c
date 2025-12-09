/**
 * @FilePath     : /CloudDisk/src/server/Communication.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-09 23:04:38
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Communication.h"

void handleMessage(int sockFd, int epFd, taskQueue_t *queue) {
    // 消息格式: cmd content
    //1.1 获取消息长度
    int length = -1;
    int ret = recvn(sockFd, &length, sizeof(length));
    if (0 == ret) {
        goto end;
    }
    printf("\n\nrecv length:%d\n", length);

    //1.2 获取消息类型
    int cmdType = -1;
    ret = recvn(sockFd, &cmdType, sizeof(cmdType));
    if (ret == 0) {
        goto end;
    }
    printf("recv cmd type: %d\n", cmdType);
    task_t *task = (task_t *)calloc(1, sizeof(task_t));
    task->peerFd = sockFd;
    task->epFd = epFd;
    task->type = cmdType;
    if (length > 0) {
        //1.3 获取消息内容
        ret = recvn(sockFd, task->data, length);
        if (ret > 0) {
            //往线程池中添加任务
            if (task->type == CMD_TYPE_PUTS) {
                //是上传文件任务，就暂时先从epoll中删除监听
                delEpollReadfd(epFd, sockFd);
            }
            taskEnque(queue, task);
        }
    } else if (length == 0) {
        taskEnque(queue, task);
    }
end:
    if (0 == ret) {
        printf("\nconn %d is closed.\n", sockFd);
        delEpollReadfd(epFd, sockFd);
        close(sockFd);
    }
}
