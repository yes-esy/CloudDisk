/**
 * @FilePath     : /CloudDisk/include/config/config.h
 * @Description  :  读取配置文件.h头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-13 20:56:08
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include "hashtable.h"
int readConfig(const char * path , HashTable * hashtable);