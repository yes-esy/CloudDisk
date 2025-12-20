/**
 * @FilePath     : /CloudDisk/src/auth/login.c
 * @Description  :  登入实现
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-20 13:48:38
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "login.h"
#include "types.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "net.h"
#include "log.h"
#include <shadow.h>
void getSetting(char *setting,const char *password) {
    int i, j;
    for (i = 0, j = 0; password[i] && j != 4; ++i) {
        if ('$' == password[i]) {
            ++j;
        }
    }
    strncpy(setting, password, i);
}
/**
 * @brief 查询用户名是否存在
 * @param user 需要查询的用户
 * @return 查询成功返回 0
 */
int selectUsername(user_info_t *user, char *response) {
    log_info("select user(%s).", user->name);
    int ret = 0;
    struct spwd *sp = getspnam(user->name);
    if (NULL == sp) {
        return -1;
    }
    char setting[256] = {0};
    strcpy(user->encrypted, sp->sp_pwdp);
    getSetting(setting, sp->sp_pwdp);
    strncpy(response,setting,strlen(setting));
    return 0;
}