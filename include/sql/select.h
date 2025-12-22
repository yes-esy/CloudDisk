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
#define SQL_LENGTH 1024
int initDatabaseConnection(RunArgs *args);
// 查询用户
int selectUser(user_info_t *user, char *response);
void DatabaseClose();