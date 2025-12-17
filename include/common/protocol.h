/**
 * @file protocol.h
 * @brief 协议解析和数据包处理
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_PROTOCOL_H
#define CLOUDDISK_PROTOCOL_H

#include "types.h"

/**
 * @brief 获取命令类型
 * @param str 命令字符串
 * @return 命令类型枚举值
 */
int getCommandType(const char *str);

/**
 * @brief 分割字符串
 * @param pstrs 源字符串
 * @param delimiter 分隔符
 * @param tokens 输出的token数组
 * @param max_tokens 最大token数量
 * @param pcount 输出的token数量
 */
void splitString(const char *pstrs, const char *delimiter, char *tokens[], 
                 int max_tokens, int *pcount);

/**
 * @brief 解析命令
 * @param pinput 输入字符串
 * @param len 输入长度
 * @param pt 输出的数据包
 * @return 成功返回0,失败返回-1
 */
int parseCommand(const char *pinput, int len, packet_t *pt,char * processedArgs);

#endif /* CLOUDDISK_PROTOCOL_H */
