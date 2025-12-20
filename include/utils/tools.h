/**
 * @FilePath     : /CloudDisk/include/utils/tools.h
 * @Description  :  工具函数
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-20 20:57:15
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <openssl/md5.h> // 用于校验和计算

/**
 * @brief 计算文件MD5校验和
 * @param filepath 文件的路径
 * @param md5_str 计算得到的MD5
 */
int calculateFileMD5(const char *filepath, char *md5_str);                               