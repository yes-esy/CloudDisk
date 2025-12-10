/**
 * @FilePath     : /CloudDisk/include/utils/LinkedList.h
 * @Description  :  链表头文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-10 21:37:16
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include "Common.h"
// 定义链表节点结构体
typedef struct ListNode {
    void *val;             // 节点的值
    struct ListNode *next; // 指向下一个节点的指针
} ListNode;

// 创建新的链表节点
ListNode *createNode(void *val);

// 在链表末尾添加元素
void appendNode(ListNode **head, void *val);

// 删除链表中值为target的节点（假设只删除一个）
void deleteNode(ListNode **head, void *target);

// 删除链表中值为peerfd的节点（假设只删除一个）
// void deleteNode2(ListNode **head, int peerfd);

// 打印链表
void printList(ListNode *head);

// 释放链表内存
void freeList(ListNode *head);