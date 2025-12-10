/**
 * @FilePath     : /CloudDisk/src/server/main.c
 * @Description  :
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 23:09:50
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#include "Common.h"
#include "Net.h"
#include "Communication.h"
#include "ThreadPool.h"
int main(int argc, char **argv) {
    (void)argv;
    //创建线程池结构体
    threadPool_t threadPool;
    memset(&threadPool, 0, sizeof(threadPool));
    //初始化线程池
    threadPoolInit(&threadPool, 3);
    //启动线程池
    threadPoolStart(&threadPool);

    ARGS_CHECK(argc, 2);
    int listenFd = tcpInit("127.0.0.1", "8080");
    int epollFd = epoll_create1(0);
    ERROR_CHECK(epollFd, -1, "epoll_create1");
    int ret = addEpollReadFd(epollFd, listenFd);
    ERROR_CHECK(ret, -1, "addEpollReadFd");
    struct epoll_event *pEventArr =
        (struct epoll_event *)calloc(EPOLL_ARR_SIZE, sizeof(struct epoll_event));
    while (1) {
        int nReady = epoll_wait(epollFd, pEventArr, EPOLL_ARR_SIZE, -1);
        if (nReady == -1 && errno == EINTR) {
            continue;
        } else if (nReady == -1) {
            ERROR_CHECK(nReady,-1,"epoll_wait");
        }
        for(int i = 0 ; i < nReady; ++ i) {
            int fd = pEventArr[i].data.fd;
            if(fd == listenFd) { // 监听
                int peerFd = accept(listenFd,NULL,NULL);
                printf("conn %d has connected.\n",peerFd);
                addEpollReadFd(epollFd,peerFd);
            } else  {
                handleMessage(fd, epollFd, &threadPool.que);
            }
        }
    }

    return 0;
}