/**
 * @file path.h
 * @brief 路径处理工具
 * @author Sheng 2900226123@qq.com
 * @version 0.2.0
 * @date 2025-12-13
 */
#ifndef CLOUDDISK_PATH_H
#define CLOUDDISK_PATH_H

#include <stddef.h>

/**
 * @brief 将真实路径转换为虚拟路径
 * @param realPath 真实路径
 * @param virtualPath 输出的虚拟路径
 * @param virtualPathSize 虚拟路径缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int realPathToVirtual(const char *realPath, char *virtualPath, size_t virtualPathSize);

/**
 * @brief 将虚拟路径转换为真实路径
 * @param virtualPath 虚拟路径
 * @param realPath 输出的真实路径
 * @param realPathSize 真实路径缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int virtualPathToReal(const char *virtualPath, char *realPath, size_t realPathSize);

int getFileFullPath(const char * fileName,const char * path,char * fileFullPath);
int getDirectoryFullPath(const char *path, const char *directoryName, char *fullPath);
int isPathAccessible(const char *pathname);
#endif /* CLOUDDISK_PATH_H */
