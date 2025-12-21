/**
 * @file Net.h
 * @brief 网络操作函数
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_NET_H
#define CLOUDDISK_NET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "macros.h"
#include "types.h"
#include "log.h"
/**
 * @brief 接收n字节数据
 * @param socketFd socket文件描述符
 * @param buff 缓冲区
 * @param len 要接收的字节数
 * @return 实际接收的字节数
 */
ssize_t recvn(int socketFd, void *buff, size_t len);

/**
 * @brief 发送n字节数据
 * @param socketFd socket文件描述符
 * @param buff 缓冲区
 * @param len 要发送的字节数
 * @return 实际发送的字节数
 */
ssize_t sendn(int socketFd, const void *buff, size_t len);

/**
 * @brief 初始化TCP服务器socket
 * @param ip IP地址
 * @param port 端口号字符串
 * @return socket文件描述符,失败返回-1
 */
int tcpInit(const char *ip, const char *port);

/**
 * @brief 连接到TCP服务器
 * @param ip IP地址
 * @param port 端口号
 * @return socket文件描述符,失败返回-1
 */
int tcpConnect(const char *ip, unsigned short port);

/**
 * @brief 添加文件描述符到epoll实例(监听读事件)
 * @param epollFd epoll文件描述符
 * @param fd 要添加的文件描述符
 * @return 成功返回0,失败返回-1
 */
int addEpollReadFd(int epollFd, int fd);

/**
 * @brief 从epoll实例删除文件描述符
 * @param epollFd epoll文件描述符
 * @param fd 要删除的文件描述符
 * @return 成功返回0,失败返回-1
 */
int delEpollReadfd(int epollFd, int fd);
/**
 * @brief 发送请求给服务器
 * @param clientFd 客户端socket
 * @param packet 数据包
 * @return 成功返回发送字节数,失败返回-1
 */
int sendRequest(int clientFd, const packet_t *packet);
/**
 * @brief 接收服务器响应(只负责接收,不处理数据)
 * @param clientFd 客户端socket
 * @param buf 接收缓冲区
 * @param bufLen 缓冲区大小
 * @param statusCode 输出参数:状态码
 * @param dataType 输出参数:数据类型
 * @return 成功返回接收的数据长度,失败返回-1,连接关闭返回0
 */
int recvResponse(int clientFd, char *buf, int bufLen, ResponseStatus *statusCode,
                 DataType *dataType);
/**
 * @brief 发送文件传输头部信息
 * @param sockfd socket
 * @param fileSize 文件大小
 * @param offset 文件偏移
 * @param fileName 文件名
 * @param checkSum 校验和
 */
int sendFileHeader(int sockfd, uint32_t fileSize, uint32_t offset, const char *fileName,
                   const char *checkSum);

#endif /* CLOUDDISK_NET_H */
