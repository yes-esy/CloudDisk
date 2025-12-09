/**
 * @FilePath     : /CloudDisk/include/utils/ThreadPool.h
 * @Description  :  线程池头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-09 23:00:37
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "Common.h"
#include <pthread.h>
#define THREAD_ERROR_CHECK(ret, funcName)                                                          \
    {                                                                                              \
        if (ret != 0) {                                                                            \
            fprintf(stderr, "%s:%s\n", funcName, strerror(ret));                                   \
        }                                                                                          \
    }

#define TASK_DATA_SIZE 1000
typedef struct task_t {
    int peerFd; // 与client进行通信的套接字
    int epFd;   // epoll实例
    char data[1000];
    struct task_t *pNext;
    CmdType type;
} task_t;
/**
 * 任务队列
 */
typedef struct taskQueue_t {
    task_t *pFront;
    task_t *pRear;
    int queSize; //记录当前任务的数量
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int flag; //0 表示要退出，1 表示不退出
} taskQueue_t;
typedef struct threadPool_t {
    pthread_t *pthreads;
    int pthreadNum;
    taskQueue_t que; //...任务队列
} threadPool_t;
// 初始化
int queueInit(taskQueue_t *queue);
// 销毁
int queueDestroy(taskQueue_t *queue);
// 判空
int queueIsEmpty(taskQueue_t *queue);
// 队列大小
int QueueSize(taskQueue_t *queue);
// 任务入队
int taskEnque(taskQueue_t *queue, task_t *task);
// 任务出队
task_t *taskDeque(taskQueue_t *queue);