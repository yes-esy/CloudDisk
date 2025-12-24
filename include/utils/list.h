/**
 * @file list.h
 * @brief 链表
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_LIST_H
#define CLOUDDISK_LIST_H

/**
 * @brief 链表节点结构
 */
typedef struct ListNode {
    void *val;             /* 节点的值 */
    struct ListNode *next; /* 指向下一个节点的指针 */
} ListNode;

/**
 * @brief 创建新的链表节点
 * @param val 节点的值
 * @return 新节点指针
 */
ListNode *createNode(void *val);

/**
 * @brief 在链表末尾添加元素
 * @param head 链表头指针的指针
 * @param val 要添加的值
 */
void appendNode(ListNode **head, void *val);

/**
 * @brief 删除链表中值为target的节点
 * @param head 链表头指针的指针
 * @param target 要删除的值
 */
void deleteNode(ListNode **head, void *target);

/**
 * @brief 打印链表(调试用)
 * @param head 链表头指针
 */
void printList(ListNode *head);

/**
 * @brief 释放链表内存
 * @param head 链表头指针
 */
void freeList(ListNode *head);

#endif /* CLOUDDISK_LIST_H */
