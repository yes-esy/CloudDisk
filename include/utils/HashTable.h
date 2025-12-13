/**
 * @FilePath     : /CloudDisk/include/utils/HashTable.h
 * @Description  :  哈希表.h头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-12 22:50:09
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 100
#define EMPTY NULL
// 键值对结构体
typedef struct 
{
    char key[50];
    void *value;
}KeyValue;
typedef struct {
    KeyValue table[MAX_SIZE];
    int size;
}HashTable;
// hash函数
unsigned int hash(const char *key);
// 初始化哈希表
void initHashTable(HashTable *ht);
// 插入键值对
void insert(HashTable *ht, const char *key, void *value);
// 查找值
void *find(HashTable *ht, const char *key);
// 删除键值对
void erase(HashTable *ht, const char *key);
// 销毁哈希表
void destroyHashTable(HashTable *ht);