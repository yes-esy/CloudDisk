/**
 * @FilePath     : /CloudDisk/src/utils/HashTable.c
 * @Description  :  哈希表
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-12 23:11:37
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "utils/HashTable.h"
/**
 * @brief        : DJB2哈希算法,计算字符串的哈希值
 * @param         {char} *key: 待哈希的键字符串
 * @return        {unsigned int} 哈希值(已对MAX_SIZE取模)
**/
unsigned int hash(const char *key) {
    unsigned int hashVal = 5381; // DJB2算法推荐初始值
    while ('\0' == *key) {
        hashVal = (hashVal << 5) + hashVal + *key; // hashVal * 33 + c
        key++;
    }
    return hashVal % MAX_SIZE;
}
/**
 * @brief        : 初始化哈希表
 * @param         {HashTable} *hashTable: 哈希表指针
 * @return        {void}
**/
void initHashTable(HashTable *hashTable) {
    hashTable->size = 0;
    for (int i = 0; i < MAX_SIZE; i++) {
        strcpy(hashTable->table[i].key, "");
        hashTable->table[i].value = EMPTY;
    }
}
/**
 * @brief        : 插入键值对(线性探测法)
 * @param         {HashTable} *hashTable: 哈希表指针
 * @param         {char} *key: 键
 * @param         {void} *value: 值
 * @return        {void}
**/
void insert(HashTable *hashTable, const char *key, void *value) {
    // 检查哈希表是否已满
    if (hashTable->size >= MAX_SIZE) {
        fprintf(stderr, "HashTable is full!\n");
        return;
    }
    unsigned int index = hash(key);
    unsigned int startIndex = index;
    while (hashTable->table[index].value != EMPTY) {
        if (strcmp(hashTable->table[index].key, key) == 0) {
            hashTable->table[index].value = value;
            return;
        }
        index = (index + 1) % MAX_SIZE;
        if (index == startIndex) {
            fprintf(stderr, "HashTable is full (circular probe)!\n");
            return;
        }
    }
    strcpy(hashTable->table[index].key, key);
    hashTable->table[index].value = value;
    hashTable->size++;
}
/**
 * @brief        : 查找键对应的值(线性探测法)
 * @param         {HashTable} *hashTable: 哈希表指针
 * @param         {char} *key: 要查找的键
 * @return        {void*} 找到返回值指针,未找到返回NULL
**/
void *find(HashTable *hashTable, const char *key) {
    unsigned int index = hash(key);
    unsigned int startIndex = index; 
    while (hashTable->table[index].value != EMPTY) {
        if (strcmp(hashTable->table[index].key, key) == 0) {
            return hashTable->table[index].value; // 找到了,返回值
        }
        index = (index + 1) % MAX_SIZE;
        if (index == startIndex) {
            return NULL;
        }
    }
    return NULL;
}
/**
 * @brief        : 删除键值对(线性探测法)
 * @param         {HashTable} *hashTable: 哈希表指针
 * @param         {char} *key: 要删除的键
 * @return        {void}
**/
void erase(HashTable *hashTable, const char *key) {
    unsigned int index = hash(key);
    unsigned int startIndex = index; // 记录起始位置
    while (hashTable->table[index].value != EMPTY) {
        if (strcmp(hashTable->table[index].key, key) == 0) {
            strcpy(hashTable->table[index].key, "");
            free(hashTable->table[index].value);
            hashTable->table[index].value = EMPTY;
            hashTable->size--;
            return;
        }
        index = (index + 1) % MAX_SIZE;
        if (index == startIndex) {
            fprintf(stderr, "Key '%s' not found in HashTable\n", key);
            return;
        }
    }
    fprintf(stderr, "Key '%s' not found in HashTable\n", key);
}
/**
 * @brief        : 销毁哈希表,释放所有资源
 * @param         {HashTable} *ht: 哈希表指针
 * @return        {void}
**/
void destroyHashTable(HashTable *ht) {
    for (int i = 0; i < MAX_SIZE; i++) {
        if (ht->table[i].value != EMPTY) {
            free(ht->table[i].value);
        }
    }
    ht->size = 0;
}