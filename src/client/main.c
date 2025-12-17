/**
 * @FilePath     : /CloudDisk/src/client/main.c
 * @Description  :
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-17 23:21:36
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#include "net.h"
#include "command.h"
#include "client.h"
#include <stdio.h>
#include <unistd.h>
#include "log.h"
#include "config.h"
int main(int argc, char **argv) {
    ARGS_CHECK(argc, 2);

    RunArgs args;
    if (runArgsLoad(&args, argv[1]) != 0) {
        fprintf(stderr, "load config failed\n");
        runArgsFree(&args);
        return 1;
    }

    // 日志
    log_init(args.logFile, LOG_DEBUG);
    log_set_quiet(true); // 禁止控制台输出
    log_info("log init finish");
    int port = atoi(args.port);
    int clientFd = tcpConnect(args.ip, port);
    char buf[1024] = {0};
    fd_set rdset;
    packet_t packet;
    print_welcome();
    while (1) {
        int nready = processClient(clientFd, &rdset);
        if (nready < 0) {
            break;
        } else if (nready == 0) {
            continue;
        }
        if (FD_ISSET(STDIN_FILENO, &rdset)) {         // 处理输入
            int ret = processStdin(buf, sizeof(buf)); //读取标准输入中的数据
            if (0 == ret) {
                print_prompt(); // 空输入时重新打印提示符
                continue;
            } else if (ret < 0) {
                break;
            }
            // printf("stdin :%s\n", buf);
            ret = processCommand(clientFd, &packet, buf);
            if (ret < 0) {
                break;
            }
        } else if (FD_ISSET(clientFd, &rdset)) {
            int ret = processServer(clientFd, buf, sizeof(buf));
            if (ret <= 0) {
                printf("\nConnection closed by server\n");
                break;
            }
            // 服务器响应处理完成后,打印新的提示符
            print_prompt();
        }
    }
    printf("\nGoodbye!\n");
    close(clientFd);
    return 0;
}