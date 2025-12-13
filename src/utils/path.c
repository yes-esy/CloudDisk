/**
 * @file path.c
 * @brief 路径处理工具实现
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#include "path.h"
#include "types.h"
#include <string.h>
#include <stdio.h>
/**
 * @brief        : 将真实路径转换为虚拟路径
 * @param         {char} *realPath: 真实路径
 * @param         {char} *virtualPath: 输出的虚拟路径
 * @param         {size_t} virtualPathSize: 虚拟路径缓冲区大小
 * @return        {int} : 成功返回0，失败返回-1
**/
int realPathToVirtual(const char *realPath, char *virtualPath, size_t virtualPathSize) {
    if (!realPath || !virtualPath || virtualPathSize == 0) {
        return -1;
    }

    // 检查真实路径是否在云盘根目录下
    size_t rootLen = strlen(CLOUD_DISK_ROOT);
    if (strncmp(realPath, CLOUD_DISK_ROOT, rootLen) != 0) {
        // 不在云盘根目录下，返回虚拟根目录
        snprintf(virtualPath, virtualPathSize, "%s", VIRTUAL_ROOT);
        return 0;
    }

    // 获取相对于云盘根目录的路径部分
    const char *relativePath = realPath + rootLen;

    if (strlen(relativePath) == 0) {
        // 如果是云盘根目录，返回虚拟根目录
        snprintf(virtualPath, virtualPathSize, "%s", VIRTUAL_ROOT);
    } else {
        // 构造虚拟路径
        if (relativePath[0] == '/') {
            snprintf(virtualPath, virtualPathSize, "%s", relativePath);
        } else {
            snprintf(virtualPath, virtualPathSize, "/%s", relativePath);
        }
    }

    return 0;
}
/**
 * @brief        : 将虚拟路径转换为真实路径
 * @param         {char} *virtualPath: 虚拟路径
 * @param         {char} *realPath: 输出的真实路径
 * @param         {size_t} realPathSize: 真实路径缓冲区大小
 * @return        {int} : 成功返回0，失败返回-1
**/
int virtualPathToReal(const char *virtualPath, char *realPath, size_t realPathSize) {
    if (!virtualPath || !realPath || realPathSize == 0) {
        return -1;
    }

    // 如果虚拟路径是根目录
    if (strcmp(virtualPath, VIRTUAL_ROOT) == 0) {
        snprintf(realPath, realPathSize, "%s", CLOUD_DISK_ROOT);
        return 0;
    }

    // 构造真实路径
    if (virtualPath[0] == '/') {
        snprintf(realPath, realPathSize, "%s%s", CLOUD_DISK_ROOT, virtualPath);
    } else {
        snprintf(realPath, realPathSize, "%s/%s", CLOUD_DISK_ROOT, virtualPath);
    }

    return 0;
}
