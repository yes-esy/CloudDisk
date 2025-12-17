/**
 * @FilePath     : /CloudDisk/src/config/config.c
 * @Description  :  读取配置文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-16 22:16:27
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return s;
}

int readConfig(const char *path, HashTable *hashtable) {
    if (path == NULL || hashtable == NULL) {
        perror("path or hashtable is NULL");
        return 1;
    }

    FILE *configFile = fopen(path, "r");
    if (configFile == NULL) {
        perror("fopen failed");
        return 2;
    }

    char line[128];
    while (fgets(line, sizeof(line), configFile) != NULL) {
        // 移除换行符
        line[strcspn(line, "\n\r")] = '\0';

        // 先 trim，再判断空行/注释
        char *p = trim(line);
        if (p[0] == '\0' || p[0] == '#') {
            continue;
        }

        char *eq = strchr(p, '=');
        if (eq == NULL || eq == p) { // 没有 '=' 或 key 为空
            continue;
        }

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (key[0] == '\0') {
            continue;
        }
        insert(hashtable, key, val);
    }

    fclose(configFile);
    return 0;
}
int runArgsLoad(RunArgs *args, const char *configPath) {
    if (!args || !configPath)
        return -1;
    memset(args, 0, sizeof(*args));

    args->ht = (HashTable *)malloc(sizeof(HashTable));
    if (!args->ht)
        return -1;
    initHashTable(args->ht);

    if (readConfig(configPath, args->ht) != 0)
        return -1;

    const char *threadNumStr = (const char *)find(args->ht, "thread_num");
    args->ip = (const char *)find(args->ht, "ip");
    args->port = (const char *)find(args->ht, "port");
    args->logFile = (const char *)find(args->ht, "log_path");
    args->workFolder = (const char *)find(args->ht, "work_folder");

    if (!threadNumStr || !args->ip || !args->port) {
        fprintf(stderr, "config missing required keys: threadNum/ip/port\n");
        return -1;
    }

    args->threadNum = atoi(threadNumStr);
    if (args->threadNum <= 0) {
        fprintf(stderr, "invalid threadNum: %s\n", threadNumStr);
        return -1;
    }

    return 0;
}

void runArgsFree(RunArgs *args) {
    if (!args)
        return;
    if (args->ht) {
        destroyHashTable(args->ht);
        free(args->ht);
    }
    memset(args, 0, sizeof(*args));
}