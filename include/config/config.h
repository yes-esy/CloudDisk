/**
 * @FilePath     : /CloudDisk/include/config/config.h
 * @Description  :  读取配置文件.h头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-16 22:16:19
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include "hashtable.h"
typedef struct {
    int threadNum;
    const char *ip;
    const char *port;
    const char *logFile;
    const char *workFolder;
    HashTable *ht; // 配置哈希表(内部value为堆内存，destroyHashTable会free)
} RunArgs;

int runArgsLoad(RunArgs *args, const char *configPath);
int readConfig(const char * path , HashTable * hashtable);
void runArgsFree(RunArgs *args);