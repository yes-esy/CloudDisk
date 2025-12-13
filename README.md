# CloudDisk - 云盘系统

一个基于C语言的客户端-服务器架构云盘系统，支持文件上传、下载、目录管理等功能。

## 项目结构

```
CloudDisk/
├── build/                  # 构建输出目录
├── config/                 # 配置文件
├── data/                   # 数据存储目录
├── docs/                   # 文档目录
├── include/                # 头文件目录
├── log/                    # 日志目录
├── scripts/                # 脚本目录
├── src/                    # 源代码目录
├── tests/                  # 测试目录
├── CMakeLists.txt         # CMake构建配置
└── README.md              # 项目说明
```

## 功能特性

- ✅ 目录管理（创建、删除、切换）
- ✅ 文件列表查询
- ✅ 多线程支持
- ✅ 日志记录
- 🚧 用户认证
- 🚧 文件上传/下载
- 🚧 文件秒传（MD5去重）
- 🚧 断点续传

## 快速开始

### 编译

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 运行服务器

```bash
./build/CloudDiskServer config/server.conf
```

### 运行客户端

```bash
./build/CloudDiskClient config/client.conf
```

## 命令说明

- \`pwd\` - 显示当前工作目录
- \`ls [path]\` - 列出目录内容
- \`cd <path>\` - 切换目录
- \`mkdir <dirname>\` - 创建目录
- \`rmdir <dirname>\` - 删除目录
- \`puts <filename>\` - 上传文件 (开发中)
- \`gets <filename>\` - 下载文件 (开发中)

## 技术栈

- **语言**: C (C11)
- **网络**: TCP Socket + Epoll
- **并发**: 线程池
- **构建**: CMake

## 文档

- [API文档](docs/API.md)
- [架构设计](docs/ARCHITECTURE.md)
- [任务清单](docs/TODO.md)

## 作者

Sheng (2900226123@qq.com)
