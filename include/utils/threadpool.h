/**
 * @file threadpool.h
 * @brief 线程池
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_THREADPOOL_H
#define CLOUDDISK_THREADPOOL_H

#include <pthread.h>
#include "types.h"
/**
 * @brief 任务队列结构
 */
typedef struct taskQueue_t {
    task_t *pFront;              /* 队首 */
    task_t *pRear;               /* 队尾 */
    int queSize;                 /* 当前任务数量 */
    pthread_mutex_t mutex;       /* 互斥锁 */
    pthread_cond_t cond;         /* 条件变量 */
    int flag;                    /* 0表示要退出,1表示不退出 */
} taskQueue_t;

/**
 * @brief 线程池结构
 */
typedef struct threadPool_t {
    pthread_t *pthreads;         /* 线程数组 */
    int pthreadNum;              /* 线程数量 */
    taskQueue_t que;             /* 任务队列 */
} threadPool_t;

/**
 * @brief 初始化任务队列
 * @param queue 任务队列指针
 * @return 成功返回0,失败返回非0
 */
int queueInit(taskQueue_t *queue);

/**
 * @brief 销毁任务队列
 * @param queue 任务队列指针
 * @return 成功返回0,失败返回非0
 */
int queueDestroy(taskQueue_t *queue);

/**
 * @brief 判断队列是否为空
 * @param queue 任务队列指针
 * @return 空返回1,非空返回0
 */
int queueIsEmpty(taskQueue_t *queue);

/**
 * @brief 获取队列大小
 * @param queue 任务队列指针
 * @return 队列中的任务数量
 */
int QueueSize(taskQueue_t *queue);

/**
 * @brief 任务入队
 * @param queue 任务队列指针
 * @param task 任务指针
 * @return 成功返回0,失败返回非0
 */
int taskEnque(taskQueue_t *queue, task_t *task);

/**
 * @brief 任务出队
 * @param queue 任务队列指针
 * @return 任务指针,队列为空返回NULL
 */
task_t *taskDeque(taskQueue_t *queue);

/**
 * @brief 初始化线程池
 * @param threadPool 线程池指针
 * @param num 线程数量
 * @return 成功返回0,失败返回非0
 */
int threadPoolInit(threadPool_t *threadPool, int num);

/**
 * @brief 启动线程池
 * @param pthreadPool 线程池指针
 * @return 成功返回0,失败返回非0
 */
int threadPoolStart(threadPool_t *pthreadPool);

#endif /* CLOUDDISK_THREADPOOL_H */
