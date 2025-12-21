/**
 * @FilePath     : /CloudDisk/src/utils/tools.c
 * @Description  :  
 * @Author       : Sheng 2900226123@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : Sheng 2900226123@qq.com
 * @LastEditTime : 2025-12-20 21:10:58
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2025.
**/
#include "tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

/**
 * @brief 计算文件MD5校验和 (使用现代EVP API)
 * @param filepath 文件路径
 * @param md5_str 输出的MD5字符串(33字节,包括\0)
 * @return 成功返回0,失败返回-1
 */
int calculateFileMD5(const char *filepath, char *md5_str) {
    if (!filepath || !md5_str) {
        return -1;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return -1;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }

    unsigned char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes) != 1) {
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return -1;
        }
    }
    fclose(file);

    unsigned char md5_hash[EVP_MAX_MD_SIZE];
    unsigned int md5_len;

    if (EVP_DigestFinal_ex(mdctx, md5_hash, &md5_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }

    EVP_MD_CTX_free(mdctx);

    // 转换为十六进制字符串
    for (unsigned int i = 0; i < md5_len; i++) {
        sprintf(md5_str + (i * 2), "%02x", md5_hash[i]);
    }
    md5_str[md5_len * 2] = '\0';

    return 0;
}
