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
#define ROOT_ID 0

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
/**
 * @brief 用户注册后初始化virtual file table
 * @param userId 用户id
 */
int initUserVirtualTable(int userId);
/**
 * @brief 向文件表中插入一个记录
 * @param file 插入的文件结构体指针
 * @return 成功返回 0，失败返回 -1
 */
int insertFile(file_t *file);
/**
 * @brief 解析路径字符串,获取目标目录的 ID
 * @param user 用户信息结构体指针
 * @param path 用户输入的路径解析成的二维字符数组
 * @param pathCount 路径段数量
 * @return 成功返回目标目录 ID,失败返回 -1
 */
int getDirectoryId(user_info_t *user, char path[][PATH_SEGMENT_MAX_LENGTH], int pathCount);
/**
 * @brief 查询路径,不存在则插入
 * @param user 操作的用户
 * @param path 解析后的路径二维数组
 * @param pathCount 路径段数量
 * @return 成功返回目标目录ID,失败返回 -1
 */
int resolveOrCreateDirectory(user_info_t *user, char path[][PATH_SEGMENT_MAX_LENGTH],
                             int pathCount);
/**
 * @brief 删除某一文件夹以及该文件夹内所有的文件夹以及文件
 * @param user 操作的用户
 * @param targetDirectId 目标路径对应id
 * @param pathCount 路径段数量
 * @return 成功返回0,失败返回 -1
 */
int deleteFiles(user_info_t *user, int targetDirectId);
/**
 * @brief 根据MD5查询文件是否存在（用于秒传）
 * @param file 文件结构体指针，调用前需填入 file->md5
 * @return 0=找到, 1=未找到, -1=错误
 */
int selectFileByMd5(file_t *file);
/**
 * @brief 文件传输过程中中断后向t_upload_task表中查询一条记录
 * @param filename 文件名
 * @param MD5 文件MD5
 */
uint64_t selectUploadTask(const char *filename, const char *MD5);
/**
 * @brief 传输过程中断向t_upload_task任务中写入一条记录
 * @param header 文件传输头
 * @param uploadedSize 已成功传输的大小
 */
int insertUploadTask(file_transfer_header_t *header, uint64_t uploadedSize);
/**
 * @brief 断点续传的文件完成传输后从此表删除记录
 * @param file_header 文件传输头
 */
int deleteUploadTask(file_transfer_header_t *file_header);
/**
 * @brief 通过路径查找文件
 * @param user 执行操作的用户
 * @param path 目标路径
 * @param 目标文件
 * @return 成功返回文件id失败返回-1
 */
int selectFileByPath(user_info_t *user, char path[][PATH_SEGMENT_MAX_LENGTH], int pathCount,
                     file_t *file);
