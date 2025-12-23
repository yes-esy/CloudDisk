#include "select.h"
#include <log.h>
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

extern ConnectionPool_T pool;
URL_T url;
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
        ret = 0;
        log_info("execute sql %s successfully", insertStr);
    }

    Connection_close(con);
    return ret;
}
