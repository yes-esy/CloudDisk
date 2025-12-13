/**
 * @file Command.h
 * @brief 命令执行函数
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_COMMAND_H
#define CLOUDDISK_COMMAND_H

#include "types.h"

/* 前向声明 */
typedef struct task_t task_t;

/**
 * @brief 执行pwd命令
 * @param task 任务结构
 */
void pwdCommand(task_t *task);

/**
 * @brief 执行ls命令
 * @param task 任务结构
 */
void lsCommand(task_t *task);

/**
 * @brief 执行cd命令
 * @param task 任务结构
 */
void cdCommand(task_t *task);

/**
 * @brief 执行mkdir命令
 * @param task 任务结构
 */
void mkdirCommand(task_t *task);

/**
 * @brief 执行rmdir命令
 * @param task 任务结构
 */
void rmdirCommand(task_t *task);

/**
 * @brief 执行puts命令(上传文件)
 * @param task 任务结构
 */
void putsCommand(task_t *task);

/**
 * @brief 执行gets命令(下载文件)
 * @param task 任务结构
 */
void getsCommand(task_t *task);

/**
 * @brief 处理未知命令
 * @param task 任务结构
 */
void notCommand(task_t *task);

/**
 * @brief 用户登录检查阶段1
 * @param task 任务结构
 */
void userLoginCheck1(task_t *task);

/**
 * @brief 用户登录检查阶段2
 * @param task 任务结构
 */
void userLoginCheck2(task_t *task);

/**
 * @brief 执行命令(命令分发器)
 * @param task 任务结构
 */
void executeCmd(task_t *task);

#endif /* CLOUDDISK_COMMAND_H */
