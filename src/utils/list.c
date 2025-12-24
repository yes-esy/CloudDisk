/**
 * @FilePath     : /CloudDisk/src/utils/list.c
 * @Description  :  链表.c文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-24 23:19:04
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "list.h"
#include <stdlib.h>
#include <stdio.h>
/**
 * @brief        : 创建一个新的节点,创建失败程序退出
 * @param         {void} *val: 新节点的值
 * @return        {ListNode*}: 新节点的指针
**/ 
ListNode *createNode(void *val) {
    ListNode *node = (ListNode *)malloc(sizeof(ListNode));
    if (NULL == node) {
        perror("Memory allocation failed");
        exit(1);
    }
    node->val = val;
    node->next = NULL;
    return node;
}

/**
 * @brief        : 链表尾部添加一个新节点
 * @param         {ListNode *} *: 链表头二级指针  
 * @param         {void} *val: 新节点的元素的值
 * @return        {voi}
**/
void appendNode(ListNode **head, void *val) {
    ListNode *node = createNode(val);
    if (NULL == *head) {
        *head = node;
        return;
    }
    ListNode *currNode = *head;
    while (currNode->next != NULL) {
        currNode = currNode->next;
    }
    currNode->next = node;
}
/**
 * @brief        : 删除链表中值为target的节点（假设只删除一个）
 * @param         {ListNode *} *: 链表头二级指针
 * @param         {void} *target: 删除目标节点的元素的值
 * @return        {void}
**/
void deleteNode(ListNode **head, void *target) {
    if (NULL == *head) {
        return;
    }
    if ((*head)->val == target) {
        ListNode *temp = *head;
        *head = (*head)->next;
        free(temp);
        return;
    }

    ListNode *current = *head;
    while (current->next != NULL && current->next->val != target) {
        current = current->next;
    }

    if (current->next != NULL) {
        ListNode *temp = current->next;
        current->next = current->next->next;
        free(temp);
    }
}

// 删除链表中值为peerfd的节点（假设只删除一个）
// void deleteNode2(ListNode **head, int peerfd) {
//     //
//     return;
// }

// 打印链表(仅供调试使用)
void printList(ListNode *head) {
    ListNode *current = head;
    while (current != NULL) {
        printf("%p ", (void *)current->val);
        current = current->next;
    }
    printf("\n");
}
/**
 * @brief        : 释放链表内存 
 * @param         {ListNode} *head: 链表头结点指针
 * @return        {void}
**/
void freeList(ListNode *head) {
    ListNode *current = head;
    while (current != NULL) {
        ListNode *temp = current;
        current = current->next;
        free(temp);
    }
}
