#include "select.h"
extern ConnectionPool_T pool;
URL_T url;
int initDatabaseConnection(RunArgs *args) {
    const char *connectionStr[128];
    snprintf(connectionStr, sizeof(connectionStr), "mysql://localhost/%s?user=%s&password=%s",
             args->database, args->username, args->password);
    url = URL_new(connectionStr);
    if (url == NULL) {
        printf("URL parse ERROR!\n");
        return 0;
    }
    ConnectionPool_T pool = ConnectionPool_new(url);
    //设置初始化连接数目
    ConnectionPool_setInitialConnections(pool, args->connectionNum);
    //开启线程池
    ConnectionPool_start(pool);
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
    snprintf(selectStr, sizeof(selectStr), "select encrypt,salt,pwd from t_user where username=%s",
             selectStr);
    ResultSet_T result = Connection_executeQuery(con, selectStr);
    if (!ResultSet_next(result)) {
        Connection_close(con);
        return -1;
    }
    const char *encrypt = ResultSet_getString(result, 1);
    const char *salt = ResultSet_getString(result, 2);
    const char *pwd = ResultSet_getString(result, 3);
    strcpy(user->encrypted, encrypt);
    strcpy(user->pwd, pwd);
    strcpy(user->salt, salt);

    Connection_close(con);
    return 0;
}

int selectUsernameUnique(user_info_t user, char *repsonse) {
    return 0;
}

int insertUser(user_info_t user, char *repsonse) {
    return 0;
}
