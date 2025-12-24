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
// 1. 定义长度常量，方便维护
#define FILE_NAME_MAX_LEN 256 // 对应数据库 VARCHAR(255)，+1用于 '\0'
#define MD5_STR_LEN 33        // 对应数据库 CHAR(32)，+1用于 '\0'

// 2. 使用枚举代替魔数，提高代码可读性
typedef enum {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_FILE = 1,  // 对应数据库 type=1
    FILE_TYPE_FOLDER = 2 // 对应数据库 type=2
} file_type_e;
/**
 * 文件结构体对应数据库一条记录
 */
typedef struct file_t {
    uint32_t id;                      // 主键id
    uint32_t parent_id;               // 上以及
    char filename[FILE_NAME_MAX_LEN]; // 文件名
    uint32_t owner_id;                // 拥有者
    char md5[MD5_STR_LEN];            // md5码
    uint64_t file_size;
    uint8_t type;       // 文件类型
    uint8_t is_deleted; // 逻辑删除：0=正常，1=回收站
    time_t create_time; // 创建时间
    time_t update_time; // 更新时间

    // char virtual_path[PATH_MAX];    // 完整的虚拟路径 (如 /a/b/c.txt)，方便前端展示
    // char physical_path[PATH_MAX];   // 磁盘物理存储路径 (如 /data/storage/00/ab/xyz)，用于读写IO

} file_t;

int initDatabaseConnection(RunArgs *args);
// 查询用户
int selectUsernameUnique(const char *username);
void DatabaseClose();
int insertUser(const char *username, const char *password);
int selectUserInfo(user_info_t *user, char *response);
int listFiles(user_info_t *user, file_t *files, int max_files);