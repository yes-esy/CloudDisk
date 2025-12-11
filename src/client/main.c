/**
 * @FilePath     : /CloudDisk/src/client/main.c
 * @Description  :
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-11 22:47:44
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#include "Common.h"
#include "Net.h"
#include "Command.h"
#include "Client.h"
int main(int argc, char **argv) {
    int clientFd = tcpConnect("127.0.0.1", 8080);
    char buf[256] = {0};
    fd_set rdset;
    packet_t packet;
    while (1) {
        int nready = processClient(clientFd, &rdset);
        if (nready < 0) {
            break;
        } else if (nready == 0) {
            continue;
        }
        if (FD_ISSET(STDIN_FILENO, &rdset)) {         // 处理输入
            int ret = processStdin(buf, sizeof(buf)); //读取标准输入中的数据
            if (0 == ret)
                continue;
            else if (ret < 0) {
                break;
            }
            printf("stdin :%s\n",buf);
            ret = processCommand(clientFd, &packet, buf);
            if (ret < 0) {
                break;
            }
        } else if (FD_ISSET(clientFd, &rdset)) {
            int ret = processServer(clientFd, buf, sizeof(buf));
            if (ret <= 0) {
                break;
            }
        }
    }
    close(clientFd);
    return 0;
}