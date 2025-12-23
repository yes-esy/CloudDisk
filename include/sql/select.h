#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zdb/zdb.h>
#include <zdb/Exception.h>
#include <zdb/Connection.h>
#include <zdb/URL.h>
#include "config.h"
#include "types.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#define SQL_LENGTH 512
#define SALT_LEN 16
#define KEY_LEN 32
#define PBKDF2_ITER 10000
int initDatabaseConnection(RunArgs *args);
// 查询用户
int selectUsernameUnique(const char *username);
void DatabaseClose();
int insertUser(const char *username, const char *password);
int selectUserInfo(user_info_t *user, char *response);