/**
 * @file hashtable.h
 * @brief 哈希表
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_HASHTABLE_H
#define CLOUDDISK_HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 100
#define EMPTY NULL

/**
 * @brief 键值对结构
 */
typedef struct {
    char key[50];
    void *value;
} KeyValue;

/**
 * @brief 哈希表结构
 */
typedef struct {
    KeyValue table[MAX_SIZE];
    int size;
} HashTable;

/**
 * @brief 初始化哈希表
 * @param ht 哈希表指针
 */
void initHashTable(HashTable *ht);

/**
 * @brief 插入键值对
 * @param ht 哈希表指针
 * @param key 键
 * @param value 值
 */
void insert(HashTable *ht, const char *key, void *value);

/**
 * @brief 查找值
 * @param ht 哈希表指针
 * @param key 键
 * @return 找到返回值指针,未找到返回NULL
 */
void *find(HashTable *ht, const char *key);

/**
 * @brief 删除键值对
 * @param ht 哈希表指针
 * @param key 键
 */
void erase(HashTable *ht, const char *key);

/**
 * @brief 销毁哈希表
 * @param ht 哈希表指针
 */
void destroyHashTable(HashTable *ht);

#endif /* CLOUDDISK_HASHTABLE_H */
