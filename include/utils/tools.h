/**
 * @FilePath     : /CloudDisk/include/utils/tools.h
 * @Description  :  工具函数
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-24 00:00:56
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#pragma once
#include <openssl/md5.h> // 用于校验和计算
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <string.h>
/**
 * @brief 计算文件MD5校验和
 * @param filepath 文件的路径
 * @param md5_str 计算得到的MD5
 */
int calculateFileMD5(const char *filepath, char *md5_str);

#define PBKDF2_ITER 10000
#define KEY_LEN 32

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