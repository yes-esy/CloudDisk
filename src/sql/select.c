#include "select.h"
#include <log.h>

extern ConnectionPool_T pool;
URL_T url;
/*
 *  derive_password_and_salt
 *  使用 OpenSSL 生成随机 salt 并用 PBKDF2-HMAC-SHA256 派生密文。
 *  salt_hex_out 长度至少为 SALT_LEN*2 + 1
 *  hash_hex_out 长度至少为 KEY_LEN*2 + 1
 *  成功返回 0，失败返回 -1
 */
int derive_password_and_salt(const char *password, char *salt_hex_out, size_t salt_hex_out_sz,
                             char *hash_hex_out, size_t hash_hex_out_sz) {
    if (!password || !salt_hex_out || !hash_hex_out)
        return -1;
    if (salt_hex_out_sz < (SALT_LEN * 2 + 1) || hash_hex_out_sz < (KEY_LEN * 2 + 1))
        return -1;

    unsigned char salt[SALT_LEN];
    unsigned char key[KEY_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1)
        return -1;

    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, SALT_LEN, PBKDF2_ITER,
                          EVP_sha256(), KEY_LEN, key)
        != 1) {
        return -1;
    }

    for (int i = 0; i < SALT_LEN; ++i) {
        snprintf(&salt_hex_out[i * 2], 3, "%02x", salt[i]);
    }
    salt_hex_out[SALT_LEN * 2] = '\0';

    for (int i = 0; i < KEY_LEN; ++i) {
        snprintf(&hash_hex_out[i * 2], 3, "%02x", key[i]);
    }
    hash_hex_out[KEY_LEN * 2] = '\0';

    return 0;
}
/**
 * @brief 初始化数据库链接
 * @param args 程序运行参数
 */
int initDatabaseConnection(RunArgs *args) {
    char connectionStr[128];
    snprintf(connectionStr, sizeof(connectionStr), "mysql://localhost/%s?user=%s&password=%s",
             args->database, args->username, args->password);
    url = URL_new(connectionStr);
    if (url == NULL) {
        printf("URL parse ERROR!\n");
        return 0;
    }
    pool = ConnectionPool_new(url);
    //设置初始化连接数目
    ConnectionPool_setInitialConnections(pool, args->connectionNum);
    //开启线程池
    ConnectionPool_start(pool);
    return 1;
}
/**
 * @brief 关闭数据库连接
 */
void DatabaseClose() {
    //将连接池与数据库分离
    ConnectionPool_stop(pool);
    ConnectionPool_free(&pool);
    URL_free(&url);
}
/**
 * @brief 查询指定用户名的信息(encrypt,salt，pwd),查询失败返回-1,成功返回0
 * @param username 用户名
 */
int selectUserInfo(user_info_t *user, char *response) {
    Connection_T con = ConnectionPool_getConnection(pool);
    char selectStr[SQL_LENGTH];
    snprintf(selectStr, sizeof(selectStr),
             "select cryptpasswd,salt,pwd from t_user where username='%s'", user->name);
    ResultSet_T result = Connection_executeQuery(con, "%s", selectStr);
    if (!ResultSet_next(result)) {
        Connection_close(con);
        return -1;
    }
    const char *cryptpasswd = ResultSet_getString(result, 1);
    const char *salt = ResultSet_getString(result, 2);
    const char *pwd = ResultSet_getString(result, 3);
    strcpy(user->encrypted, cryptpasswd);
    strcpy(user->pwd, pwd);
    strcpy(user->salt, salt);
    strncpy(response, salt, strlen(salt));
    Connection_close(con);
    return 0;
}
/**
 * @brief 判断某一用户名是否唯一
 * @param usernme 带判断的用户名
 */
int selectUsernameUnique(const char *username) {
    Connection_T con = ConnectionPool_getConnection(pool);
    ResultSet_T result =
        Connection_executeQuery(con, "select * from t_user where username='%s'", username);
    int ret = -1;
    if (!ResultSet_next(result)) {
        ret = 0;
    }
    Connection_close(con);
    return ret;
}
/**
 * @brief 用户注册后初始化virtual file table
 * @param userId 用户id
 */
int initUserVirtualTable(int userId) {
    if (userId < 0) {
        log_error("invalid user id,please check.");
        return -1;
    }
    file_t file;
    memset(&file, 0, sizeof(file_t));
    file.owner_id = (uint32_t)userId;
    strcpy(file.filename, "/");
    // 显式初始化 md5 为空字符串，防止 SQL 读取垃圾值
    strcpy(file.md5, "");
    file.file_size = 4096;
    file.is_deleted = 0;
    file.parent_id = 0;
    file.type = 2;
    int ret = insertFile(&file);
    if (ret != 0) {
        log_error("initialize user virtual table failed (insertFile returned %d).", ret);
        return -1; // 插入失败，返回 -1
    }
    return (int)file.id;
}
/**
 * @brief 插入一个用户
 * @param username 用户名
 * @param password 明文密码
 */
int insertUser(const char *username, const char *password) {
    int ret = -1;
    Connection_T con = ConnectionPool_getConnection(pool);
    if (!con)
        return -1;

    char salt_hex[SALT_LEN * 2 + 1];
    char hash_hex[KEY_LEN * 2 + 1];

    if (derive_password_and_salt(password, salt_hex, sizeof(salt_hex), hash_hex, sizeof(hash_hex))
        != 0) {
        Connection_close(con);
        return -1;
    }

    /* 构建并执行 SQL（按项目现有风格；注意：此处没有做 SQL 注入防护，建议后续改为参数化查询） */
    char insertStr[SQL_LENGTH];
    snprintf(insertStr, sizeof(insertStr),
             "insert into t_user(username,salt,cryptpasswd,pwd) values('%s','%s','%s','/')",
             username, salt_hex, hash_hex);
    log_info("execute sql %s", insertStr);

    ResultSet_T result = Connection_executeQuery(con, "%s", insertStr);
    if (result) {
        ret = Connection_lastRowId(con);
        log_info("execute sql %s successfully", insertStr);
    }

    Connection_close(con);
    return ret;
}
/**
 * @brief 向文件表中插入一个记录
 * @param file 插入的文件结构体指针
 * @return 成功返回 0，失败返回 -1
 */
int insertFile(file_t *file) {
    // 1. 获取连接
    Connection_T con = ConnectionPool_getConnection(pool);
    if (con == NULL) {
        log_error("Get connection failed");
        return -1;
    }
    int ret = -1;
    // 使用 TRY-CATCH 捕获数据库异常（ZDB 推荐做法）
    TRY {
        // 2. 执行 INSERT
        // 注意：这里不使用 snprintf，直接传参！
        // 底层会自动处理字符串转义（防止文件名带单引号报错）
        // file_size 强转 (unsigned long long) 并使用 %llu 确保大文件正确
        Connection_execute(
            con,
            "INSERT INTO virtual_fs(parent_id, filename, owner_id, md5, file_size, type) "
            "VALUES(%u, '%s', %u, '%s', %llu, %u)",
            (unsigned int)file->parent_id,
            file->filename, // 底层自动转义
            (unsigned int)file->owner_id,
            file->md5,                           // 底层自动转义
            (unsigned long long)file->file_size, // 防止大文件溢出
            (unsigned int)file->type);
        // 3. 获取自增 ID (这是非常重要的一步)
        // 刚插入的记录 ID 需要回填到结构体中，以便后续操作
        long long last_id = Connection_lastRowId(con);
        if (last_id > 0) {
            file->id = (uint32_t)last_id;
            ret = 0;
            log_info("Insert file success, id=%u, name=%s", file->id, file->filename);
        } else {
            log_error("Insert failed but no exception, id=0");
        }
    }
    CATCH(SQLException) {
        // 捕获 SQL 执行异常
        log_error("SQL Error: %s", Exception_frame.message);
        ret = -1;
    }
    END_TRY;
    // 4. 关闭连接
    Connection_close(con);
    return ret;
}
/**
 * @brief 根据文件ID查询文件信息
 * @param file 文件结构体指针，调用前需填入 file->id
 * @return 0=成功找到, 1=记录不存在, -1=数据库错误
 */
int selectFileById(file_t *file) {
    if (file == NULL || file->id == 0) {
        return -1;
    }

    Connection_T con = ConnectionPool_getConnection(pool);
    if (con == NULL) {
        log_error("Get connection failed");
        return -1;
    }

    int ret = -1;

    TRY {
        // 执行查询
        // 注意：这里我们显式列出字段名，而不是 SELECT *，这在表结构变更时更安全
        ResultSet_T result = Connection_executeQuery(
            con,
            "SELECT id, parent_id, filename, owner_id, md5, file_size, type "
            "FROM virtual_fs WHERE id = %u",
            (unsigned int)file->id);

        // 尝试获取第一行
        if (ResultSet_next(result)) {
            // 这里的索引从 1 开始，对应 SQL 中字段的顺序

            // 1. id (已经在输入中，可跳过或重新赋值)
            // file->id = ResultSet_getInt(result, 1);

            // 2. parent_id
            file->parent_id = (uint32_t)ResultSet_getInt(result, 2);

            // 3. filename (注意字符串长度安全)
            const char *fname = ResultSet_getString(result, 3);
            if (fname) {
                snprintf(file->filename, sizeof(file->filename), "%s", fname);
            }

            // 4. owner_id
            file->owner_id = (uint32_t)ResultSet_getInt(result, 4);

            // 5. md5
            const char *md5_val = ResultSet_getString(result, 5);
            if (md5_val) {
                snprintf(file->md5, sizeof(file->md5), "%s", md5_val);
            } else {
                memset(file->md5, 0, sizeof(file->md5));
            }

            // 6. file_size (注意使用 getLLong 获取 64位整数)
            file->file_size = (uint64_t)ResultSet_getLLong(result, 6);

            // 7. type
            file->type = (uint8_t)ResultSet_getInt(result, 7);

            ret = 0; // 查询成功
            log_info("Select file success, id=%u", file->id);
        } else {
            ret = 1; // 未找到记录
            log_info("File not found, id=%u", file->id);
        }
    }
    CATCH(SQLException) {
        log_error("SQL Error: %s", Exception_frame.message);
        ret = -1;
    }
    END_TRY;

    Connection_close(con);
    return ret;
}
/**
 * @brief 根据MD5查询文件是否存在（用于秒传）
 * @param file 文件结构体指针，调用前需填入 file->md5
 * @return 0=找到, 1=未找到, -1=错误
 */
int selectFileByMd5(file_t *file) {
    if (file == NULL || strlen(file->md5) == 0) {
        return -1;
    }

    Connection_T con = ConnectionPool_getConnection(pool);
    if (con == NULL)
        return -1;

    int ret = -1;

    TRY {
        // 使用 LIMIT 1，因为我们只需要知道有没有，或者取第一条记录即可
        ResultSet_T result = Connection_executeQuery(
            con, "SELECT id, file_size, owner_id FROM virtual_fs WHERE md5 = '%s' LIMIT 1",
            file->md5);

        if (ResultSet_next(result)) {
            // 如果存在，把 ID 拿出来即可
            file->id = (uint32_t)ResultSet_getInt(result, 1);
            // 可选：记录该文件的大小和所有者，用于逻辑判断
            file->file_size = (uint64_t)ResultSet_getLLong(result, 2);
            file->owner_id = (uint32_t)ResultSet_getInt(result, 3);

            ret = 0; // 存在
        } else {
            ret = 1; // 不存在
        }
    }
    CATCH(SQLException) {
        log_error("SQL Error: %s", Exception_frame.message);
        ret = -1;
    }
    END_TRY;

    Connection_close(con);
    return ret;
}
/**
 * @brief 列出当前目录下的文件 (单次查询版)
 * @param user 用户信息，内部包含 current_dir_id
 * @param files 文件数组
 * @param max_files 数组最大容量
 * @return 成功返回文件数量，失败返回 -1
 */
int listFiles(user_info_t *user, file_t *files, int max_files) {
    if (NULL == user || NULL == files || max_files <= 0) {
        return -1;
    }

    // 1. 直接使用 user 中缓存的 ID，不需要再去解析路径字符串
    // 如果是根目录，current_dir_id 通常为 0 或 1
    int parent_id = user->current_dir_id;
    Connection_T con = ConnectionPool_getConnection(pool);
    if (!con)
        return -1;

    int count = 0;
    ResultSet_T result = NULL;

    TRY {
        // 2. 执行单次查询
        // 直接命中联合索引 (owner_id, parent_id)
        result = Connection_executeQuery(con,
                                         "SELECT "
                                         "id, parent_id, filename, owner_id, md5, file_size, type "
                                         "FROM virtual_fs "
                                         "WHERE owner_id = %u "
                                         "AND parent_id = %u "
                                         "AND is_deleted = 0 "
                                         "ORDER BY type ASC, filename ASC",
                                         (unsigned int)user->user_id, (unsigned int)parent_id);

        // 3. 填充结果
        while (ResultSet_next(result)) {
            if (count >= max_files)
                break;
            file_t *f = &files[count];
            f->id = (uint32_t)ResultSet_getInt(result, 1);
            f->parent_id = (uint32_t)ResultSet_getInt(result, 2);
            const char *fname = ResultSet_getString(result, 3);
            if (fname)
                snprintf(f->filename, sizeof(f->filename), "%s", fname);
            f->owner_id = (uint32_t)ResultSet_getInt(result, 4);
            const char *md5 = ResultSet_getString(result, 5);
            if (md5)
                snprintf(f->md5, sizeof(f->md5), "%s", md5);
            f->file_size = (uint64_t)ResultSet_getLLong(result, 6);
            f->type = (uint8_t)ResultSet_getInt(result, 7);
            count++;
        }
    }
    CATCH(SQLException) {
        log_error("SQL Error: %s", Exception_frame.message);
        count = -1;
    }
    END_TRY;
    Connection_close(con);
    return count;
}
/**
 * @brief 解析路径字符串，获取目标目录的 ID
 * @param user 用户信息结构体指针
 * @param path_str 用户输入的路径
 * @return 成功返回目标目录 ID，失败返回 -1
 */
int getDirectoryId(user_info_t *user, char *path_str) {
    // 1. 参数检查
    if (!user || !path_str)
        return -1;
    Connection_T con = ConnectionPool_getConnection(pool);
    if (!con)
        return -1;
    // 2. 复制路径字符串
    char *path_copy = strdup(path_str);
    if (!path_copy) {
        Connection_close(con);
        return -1;
    }
    uint32_t target_id = user->current_dir_id;
    char *token;
    const char *delim = "/";
    // 3. 判断绝对路径
    if (path_str[0] == '/') {
        target_id = 0; // 回到根目录 ID
    }
    token = strtok(path_copy, delim);
    while (token != NULL) {
        // 处理 "."
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, delim);
            continue;
        }
        // 处理 ".."
        if (strcmp(token, "..") == 0) {
            // 优化：如果在根目录，直接跳过查询
            if (target_id != 0) {
                ResultSet_T rs =
                    Connection_executeQuery(con,
                                            "SELECT parent_id "
                                            "FROM virtual_fs "
                                            "WHERE id = %u AND type = 2 AND owner_id = %d",
                                            target_id, user->user_id);
                if (rs && ResultSet_next(rs)) {
                    target_id = (uint32_t)ResultSet_getInt(rs, 1);
                }
            }
            token = strtok(NULL, delim);
            continue;
        }
        // 4. 处理普通目录名
        ResultSet_T rs = Connection_executeQuery(
            con,
            "SELECT id "
            "FROM virtual_fs "
            "WHERE parent_id = %u AND filename = '%s' AND type = 2 AND owner_id = %d",
            target_id, token, user->user_id);

        int found = 0;
        if (rs) {
            if (ResultSet_next(rs)) {
                target_id = (uint32_t)ResultSet_getInt(rs, 1);
                found = 1;
            }
        }
        if (!found) {
            log_error("Path component not found: %s (parent_id=%u)", token, target_id);
            free(path_copy);
            Connection_close(con); // 别忘了关闭连接
            return -1;
        }
        token = strtok(NULL, delim);
    }

    free(path_copy);
    Connection_close(con);
    // 返回最终的 ID
    return (int)target_id;
}
/**
 * @brief 查询路径，不存在则插入
 * @param user 操作的用户
 * @param path 解析后的路径数组
 * @param con 数据库连接对象
 * @return 成功返回目标目录ID，失败返回 -1
 */
int resolveOrCreateDirectory(user_info_t *user, const char **path) {
    if (!user || !path) {
        return -1;
    }
    int targetDirectId = -1;
    // 1. 确定起始目录 ID
    if (strcmp(path[0], ".")) {
        targetDirectId = user->current_dir_id;
    } else if (strcmp(path[0], "/")) {
        targetDirectId = ROOT_ID;
    } else {
        // 路径解析逻辑错误，理应只有 "." 或 "/" 开头，或者这里做兼容处理
        return -1;
    }
    int idx = 1;
    Connection_T con = ConnectionPool_getConnection(pool);
    while (path[idx] != NULL) {
        ResultSet_T selectRes =
            Connection_executeQuery(con,
                                    "SELECT id FROM virtual_fs "
                                    "WHERE parent_id = %d "
                                    "AND owner_id = %u  "
                                    "AND type = 2 "
                                    "AND filename = '%s'",
                                    targetDirectId, (unsigned int)user->user_id, path[idx]);

        if (ResultSet_next(selectRes)) {
            // 目录存在，获取 ID 并进入下一层
            targetDirectId = (int)ResultSet_getInt(selectRes, 1);
        } else {
            // 目录不存在，插入新目录
            // 注意：这里假设 Connection_execute 失败会抛出异常或至少我们能拿到 LastRowId
            // 建议增加 try-catch (如果 libzdb 开启了异常模式) 或检查返回值
            TRY {
                Connection_execute(
                    con,
                    "INSERT INTO virtual_fs(parent_id, filename, owner_id, md5, file_size, type) "
                    "VALUES(%u, '%s', %u, '%s', %llu, 2)",
                    (unsigned int)targetDirectId, path[idx], (unsigned int)user->user_id,
                    " ",                     // md5 占位
                    (unsigned long long)4096 // 默认大小
                );
                // 获取刚插入行的 ID
                targetDirectId = (int)Connection_lastRowId(con);
            }
            CATCH(SQLException) {
                log_error("insert error:%s", SQLException.name);
                return -1;
            }
            END_TRY;
        }
        idx++;
    }

    return targetDirectId;
}
