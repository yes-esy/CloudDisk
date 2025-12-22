/**
 * @FilePath     : /CloudDisk/src/server/main.c
 * @Description  :
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-21 21:26:28
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#include "net.h"
#include "handler.h"
#include "threadpool.h"
#include "config.h"
#include "hashtable.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "list.h"
#include "select.h"
ListNode *userList = NULL;
ConnectionPool_T pool = NULL;
int main(int argc, char **argv) {
    ARGS_CHECK(argc, 2);
    RunArgs args;
    if (runArgsLoad(&args, argv[1]) != 0) {
        fprintf(stderr, "load config failed\n");
        runArgsFree(&args);
        return 1;
    }
    initDatabaseConnection(&args);
    // 日志
    log_init(args.logFile, LOG_DEBUG);
    log_info("log init finish");

    // 创建/启动线程池
    threadPool_t threadPool;
    memset(&threadPool, 0, sizeof(threadPool));
    threadPoolInit(&threadPool, args.threadNum);
    threadPoolStart(&threadPool);

    // 使用配置的 ip/port
    int listenFd = tcpInit(args.ip, args.port);

    int epollFd = epoll_create1(0);
    ERROR_CHECK(epollFd, -1, "epoll_create1");

    int ret = addEpollReadFd(epollFd, listenFd);
    ERROR_CHECK(ret, -1, "addEpollReadFd");

    struct epoll_event *pEventArr =
        (struct epoll_event *)calloc(EPOLL_ARR_SIZE, sizeof(struct epoll_event));
    if (!pEventArr) {
        perror("calloc");
        runArgsFree(&args);
        return 2;
    }

    while (1) {
        int nReady = epoll_wait(epollFd, pEventArr, EPOLL_ARR_SIZE, -1);
        if (nReady == -1 && errno == EINTR) {
            continue;
        } else if (nReady == -1) {
            ERROR_CHECK(nReady, -1, "epoll_wait");
        }
        for (int i = 0; i < nReady; ++i) {
            int fd = pEventArr[i].data.fd;
            if (fd == listenFd) { // 监听
                int peerFd = accept(listenFd, NULL, NULL);
                if (peerFd >= 0) {
                    log_info("conn %d has connected.\n", peerFd);
                    addEpollReadFd(epollFd, peerFd);
                    user_info_t *userNode = (user_info_t *)calloc(1, sizeof(user_info_t));
                    if (userNode) {
                        userNode->sockfd = peerFd;
                        appendNode(&userList, userNode);
                    }
                }
            } else {
                handleMessage(fd, epollFd, &threadPool.que);
            }
        }
    }

    // 当前死循环一般不会走到这里；将来加退出逻辑时记得释放：
    // free(pEventArr);
    // close(epollFd);
    // close(listenFd);
    // runArgsFree(&args);
}