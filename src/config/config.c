/**
 * @FilePath     : /CloudDisk/src/config/config.c
 * @Description  :  读取配置文件
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-13 16:33:45
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