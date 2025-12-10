/**
 * @FilePath     : /CloudDisk/src/client/main.c
 * @Description  :
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 23:30:02
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
 **/
#include "Common.h"
#include "Net.h"
#include "Command.h"
int main(int argc, char **argv) {
    (void)argv;
    ARGS_CHECK(argc, 3);
    int clientfd = tcpConnect("127.0.0.1", 8080);
    char buf[128] = {0};
    fd_set rdset;
    packet_t packet;
    while (1) {
        FD_ZERO(&rdset);
        FD_SET(STDIN_FILENO, &rdset);
        FD_SET(clientfd, &rdset);

        int nready = select(clientfd + 1, &rdset, NULL, NULL, NULL);
        if (nready < 0) {
            perror("select");
            break;
        } else if (nready == 0) {
            continue;
        }
        if (FD_ISSET(STDIN_FILENO, &rdset)) {
            //读取标准输入中的数据
            memset(buf, 0, sizeof(buf));
            ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (ret < 0) {
                perror("read stdin");
                break;
            }
            if (ret == 0) { /* EOF */
                printf("byebye.\n");
                break;
            }
            /* 去掉末尾换行（如果有）并保证不越界 */
            if (ret > 0 && buf[ret - 1] == '\n') {
                buf[ret - 1] = '\0';
            } else {
                buf[ret] = '\0';
            }
            memset(&packet, 0, sizeof(packet_t));
            parseCommand(buf, strlen(buf), &packet);
            /* 发送封包（4+4+packet.len 是原实现的长度计算，保持不变） */
            if (sendn(clientfd, &packet, 4 + 4 + packet.len) < 0) {
                perror("sendn");
                break;
            }
            if (packet.cmdType == CMD_TYPE_PUTS) {
                // putsCommand(clientfd, &packet);
                // @todo 发送文件
            }
        } else if (FD_ISSET(clientfd, &rdset)) {
            ssize_t r = recv(clientfd, buf, sizeof(buf) - 1, 0);
            if (r < 0) {
                perror("recv");
                break;
            } else if (r == 0) {
                printf("server closed connection\n");
                break;
            } else {
                buf[r] = '\0';
                printf("recv:%s\n", buf);
            }
        }
    }
    close(clientfd);
    return 0;
}