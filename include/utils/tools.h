/**
 * @FilePath     : /CloudDisk/include/utils/tools.h
 * @Description  :  工具函数
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-26 22:09:29
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <openssl/md5.h> // 用于校验和计算
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <string.h>
#include <types.h>
#define PBKDF2_ITER 10000
#define KEY_LEN 32
/**
 * @brief 计算文件MD5校验和
 * @param filepath 文件的路径
 * @param md5_str 计算得到的MD5
 */
int calculateFileMD5(const char *filepath, char *md5_str);


/**
 * @brief 使用服务器返回的 salt 派生密码
 */
static int derive_password_with_salt(const char *password, const unsigned char *salt_bin,
                                     size_t salt_len, char *hash_hex_out, size_t hash_hex_out_sz) {
    if (!password || !salt_bin || !hash_hex_out)
        return -1;
    if (hash_hex_out_sz < (KEY_LEN * 2 + 1))
        return -1;

    unsigned char key[KEY_LEN];

    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt_bin, salt_len, PBKDF2_ITER,
                          EVP_sha256(), KEY_LEN, key)
        != 1) {
        return -1;
    }

    // 转换为十六进制字符串
    for (int i = 0; i < KEY_LEN; ++i) {
        snprintf(&hash_hex_out[i * 2], 3, "%02x", key[i]);
    }
    hash_hex_out[KEY_LEN * 2] = '\0';

    return 0;
}

/**
 * @brief 将十六进制字符串转换为二进制
 */
static int hex_to_bin(const char *hex, unsigned char *bin, size_t bin_len) {
    size_t hex_len = strlen(hex);
    if (hex_len != bin_len * 2)
        return -1;

    for (size_t i = 0; i < bin_len; i++) {
        sscanf(&hex[i * 2], "%2hhx", &bin[i]);
    }
    return 0;
}
/**
 * @brief 解析路径将./a/b/c和/a/b/c以及a/b/c统一解析成/a/b/c/d (线程安全版本)
 * @param targetPath 待解析的路径
 * @param pathArray 存放解析后的路径的二维字符数组
 * @param pwd 当前工作目录绝对路径
 * @return 路径段数量(>=0成功), -1失败(回退过多或参数错误)
 */
static int parsePathToArray(const char *targetPath,
                            char pathArray[][PATH_SEGMENT_MAX_LENGTH],
                            const char *pwd) {
    if (NULL == targetPath || NULL == pathArray) {
        if (pathArray)
            pathArray[0][0] = '\0';
        return -1;
    }

    int idx = 0;
    char path_copy[1024];
    char *saveptr1 = NULL; // strtok_r 的状态指针
    char *saveptr2 = NULL; // strtok_r 的状态指针

    strncpy(path_copy, targetPath, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // 处理绝对路径和相对路径
    if (path_copy[0] == '/') {
        // 绝对路径:直接从根目录开始
        strncpy(pathArray[idx++], "/", PATH_SEGMENT_MAX_LENGTH - 1);
        pathArray[idx - 1][PATH_SEGMENT_MAX_LENGTH - 1] = '\0';
    } else {
        // 相对路径:需要基于当前工作目录
        if (pwd && pwd[0] == '/') {
            strncpy(pathArray[idx++], "/", PATH_SEGMENT_MAX_LENGTH - 1);
            pathArray[idx - 1][PATH_SEGMENT_MAX_LENGTH - 1] = '\0';

            // 解析pwd到pathArray
            char pwd_copy[1024];
            strncpy(pwd_copy, pwd + 1, sizeof(pwd_copy) - 1);
            pwd_copy[sizeof(pwd_copy) - 1] = '\0';

            char *token = strtok_r(pwd_copy, "/", &saveptr1);
            while (token != NULL && idx < PATH_MAX_SEGMENTS - 1) {
                strncpy(pathArray[idx], token, PATH_SEGMENT_MAX_LENGTH - 1);
                pathArray[idx][PATH_SEGMENT_MAX_LENGTH - 1] = '\0';
                idx++;
                token = strtok_r(NULL, "/", &saveptr1);
            }
        }
    }

    // 解析目标路径
    char *token = strtok_r(path_copy[0] == '/' ? path_copy + 1 : path_copy, "/", &saveptr2);
    while (token != NULL && idx < PATH_MAX_SEGMENTS - 1) {
        if (strcmp(token, ".") == 0) {
            // 当前目录,跳过
        } else if (strcmp(token, "..") == 0) {
            // 上级目录,回退一级
            if (idx > 1) {
                idx--;
            } else {
                // 回退过多,返回错误
                pathArray[0][0] = '\0';
                return -1;
            }
        } else if (strlen(token) > 0) {
            // 普通目录名
            strncpy(pathArray[idx], token, PATH_SEGMENT_MAX_LENGTH - 1);
            pathArray[idx][PATH_SEGMENT_MAX_LENGTH - 1] = '\0';
            idx++;
        }
        token = strtok_r(NULL, "/", &saveptr2);
    }

    // 标记结束
    if (idx < PATH_MAX_SEGMENTS) {
        pathArray[idx][0] = '\0';
    }

    return idx;
}