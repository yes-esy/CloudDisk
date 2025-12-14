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

/**
 * @brief 获取文件完整路径
 * @param fileName 文件名
 * @param path 目录路径
 * @param fileFullPath 输出的完整路径
 */
void getFileFullPath(const char *fileName, const char *path, char *fileFullPath) {
    if (fileName == NULL || path == NULL || fileFullPath == NULL) {
        return;
    }

    size_t pathLen = strlen(path);

    // 检查路径是否以 / 结尾
    if (pathLen > 0 && path[pathLen - 1] == '/') {
        // 路径已经有 /,直接拼接
        snprintf(fileFullPath, PATH_MAX_LENGTH, "%s%s", path, fileName);
    } else {
        // 路径没有 /,需要添加
        snprintf(fileFullPath, PATH_MAX_LENGTH, "%s/%s", path, fileName);
    }
}

/**
 * @brief 获取目录完整路径
 * @param path 当前目录路径
 * @param directoryName 目录名
 * @param fullPath 输出的完整路径
 * @return 成功返回0,失败返回-1
 */
int getDirectoryFullPath(const char *path, const char *directoryName, char *fullPath) {
    if (!path || !directoryName || !fullPath) {
        return -1;
    }

    // 检查目录名是否为空
    if (directoryName[0] == '\0') {
        return -1;
    }

    size_t pathLen = strlen(path);
    size_t nameLen = strlen(directoryName);

    // 检查长度是否会溢出
    if (pathLen + nameLen + 2 > PATH_MAX_LENGTH) {
        fullPath[0] = '\0';
        return -1;
    }

    // 处理绝对路径
    if (directoryName[0] == '/') {
        snprintf(fullPath, PATH_MAX_LENGTH, "%s", directoryName);
        return 0;
    }

    // 拼接路径
    if (pathLen > 0 && path[pathLen - 1] == '/') {
        // 路径已经有 /,直接拼接
        snprintf(fullPath, PATH_MAX_LENGTH, "%s%s", path, directoryName);
    } else {
        // 路径没有 /,需要添加
        snprintf(fullPath, PATH_MAX_LENGTH, "%s/%s", path, directoryName);
    }

    return 0;
}