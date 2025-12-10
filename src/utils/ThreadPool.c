/**
 * @FilePath     : /CloudDisk/src/utils/ThreadPool.c
 * @Description  :  线程池.c文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 21:50:38
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "ThreadPool.h"
//每一个子线程在执行的函数执行体(start_routine)
void *threadFunc(void *arg) {
    //不断地从任务队列中获取任务，并执行
    threadPool_t *threadPool = (threadPool_t *)arg;
    while (1) {
        task_t *ptask = taskDeque(&threadPool->que);
        if (ptask) {
            //执行业务逻辑
            executeCmd(ptask);
            free(ptask); //执行完任务后，释放任务节点
        } else {         //ptask为NULL的情况
            break;
        }
    }
    printf("sub thread %ld is exiting.\n", pthread_self());
    return NULL;
}
int threadPoolInit(threadPool_t *threadPool, int num) {
    threadPool->pthreadNum = num;
    threadPool->pthreads = calloc(num, sizeof(pthread_t));
    queueInit(&threadPool->que);

    return 0;
}
int threadPoolStart(threadPool_t *threadPool) {
    if (threadPool) {
        for (int i = 0; i < threadPool->pthreadNum; ++i) {
            int ret = pthread_create(&threadPool->pthreads[i], NULL, threadFunc, threadPool);
            THREAD_ERROR_CHECK(ret, "pthread_create");
        }
    }
    return 0;
}

/**
 * @brief        : 对任务队列进行初始化
 * @param         {taskQueue_t} *queue: 任务队列
 * @return        {int} : 0成功,其它失败
**/
int queueInit(taskQueue_t *queue) {
    if (queue == NULL) {
        perror("queue init failed,queue is null.");
        return -1;
    }
    queue->pFront = queue->pRear = NULL;
    queue->queSize = 0;
    queue->flag = 1;
    int ret = pthread_mutex_init(&queue->mutex, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_init");
    ret = pthread_cond_init(&queue->cond, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_cond_init");
    return 0;
}
/**
 * @brief        : 对任务队列进行销毁
 * @param         {taskQueue_t} *queue: 任务队列
 * @return        {int} : 0成功,其它失败
**/
int queueDestroy(taskQueue_t *queue) {
    if (queue == NULL) {
        perror("queue destroy failed,queue is null.");
        return -1;
    }
    while (queue->queSize != 0) {
    }
    int ret = pthread_mutex_destroy(&queue->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_destroy");
    ret = pthread_cond_destroy(&queue->cond);
    THREAD_ERROR_CHECK(ret, "pthread_cond_destroy");
    return 0;
}
/**
 * @brief        : 判断任务队列是否为空
 * @param         {taskQueue_t} *queue: 任务队列
 * @return        {int} 1空,0非空
**/
int queueIsEmpty(taskQueue_t *queue) {
    return queue->queSize == 0;
}
/**
 * @brief        : 队列大小
 * @param         {taskQueue_t} *queue:任务队列
 * @return        {int} : 任务队列任务数量
**/
int QueueSize(taskQueue_t *queue) {
    return queue->queSize;
}
// 任务入队
/**
 * @brief        : 任务入队
 * @param         {taskQueue_t} *queue:任务队列
 * @param         {task_t} *task: 任务
 * @return        {int} : 状态码0成功，其他失败
**/
int taskEnque(taskQueue_t *queue, task_t *task) {
    int ret = pthread_mutex_lock(&queue->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
    if (queueIsEmpty(queue)) {
        queue->pFront = queue->pRear = task;
    } else {
        task->pNext = queue->pFront;
        queue->pFront = task;
    }
    queue->queSize++;
    ret = pthread_mutex_unlock(&queue->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
    // 通知消费者消费
    ret = pthread_cond_signal(&queue->cond);
    THREAD_ERROR_CHECK(ret, "pthread_cond_signal");
    return 0;
}
/**
 * @brief        : 任务出队
 * @param         {taskQueue_t} *queue: 任务队列
 * @return        {task_t *} : 出队的任务
**/
task_t *taskDeque(taskQueue_t *queue) {
    int ret = pthread_mutex_lock(&queue->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
    task_t *task;
    //虚假唤醒
    while (queue->flag && queueIsEmpty(queue)) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    if (1 == queue->flag) {
        task = queue->pFront;
        if (queue->pFront == queue->pRear) {
            queue->pFront = queue->pRear = NULL;
        } else {
            queue->pFront = queue->pFront->pNext;
        }
        queue->queSize--;
    } else {
        task = NULL;
    }
    ret = pthread_mutex_unlock(&queue->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
    return task;
}

// 主线程调用
/**
 * @brief        : 通知所有线程
 * @param         {task_queue_t} *queue: 任务队列
 * @return        {int} : 状态码0成功，其他失败
**/
int broadcastALL(taskQueue_t *queue) {
    // 先修改要退出的标识位
    queue->flag = 0;
    int ret = pthread_cond_broadcast(&queue->cond);
    THREAD_ERROR_CHECK(ret, "pthread_cond_broadcast");
    return 0;
}