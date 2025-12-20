cmake_minimum_required(VERSION 3.26)
project(CloudDisk C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -Wno-unused-result
)

# 导出compile_commands.json for IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 源文件目录
set(CLIENT_SRC "src/client")
set(SERVER_SRC "src/server")
set(COMMON_SRC "src/common")
set(UTILS_SRC "src/utils")
set(CONFIG_SRC "src/config")
set(LOGIN_SRC "src/auth")

# 客户端源文件
set(CLIENT_SRCS
    "${CLIENT_SRC}/main.c"
    "${CLIENT_SRC}/client.c"
    "${COMMON_SRC}/net.c"
    "${COMMON_SRC}/protocol.c"
    "${UTILS_SRC}/hashtable.c"
    "${UTILS_SRC}/log.c"
    "${CONFIG_SRC}/config.c"
)

# 服务器源文件
set(SERVER_SRCS
    "${SERVER_SRC}/main.c"
    "${SERVER_SRC}/handler.c"
    "${COMMON_SRC}/command.c"
    "${COMMON_SRC}/net.c"
    "${COMMON_SRC}/protocol.c"
    "${UTILS_SRC}/threadpool.c"
    "${UTILS_SRC}/list.c"
    "${UTILS_SRC}/hashtable.c"
    "${UTILS_SRC}/path.c"
    "${UTILS_SRC}/log.c"
    "${CONFIG_SRC}/config.c"
    "${LOGIN_SRC}/login.c"
)

# 生成服务器可执行文件
add_executable(CloudDiskServer ${SERVER_SRCS})

# 生成客户端可执行文件
add_executable(CloudDiskClient ${CLIENT_SRCS})

# 服务器包含目录
target_include_directories(CloudDiskServer PRIVATE
    ${PROJECT_SOURCE_DIR}/include/common
    ${PROJECT_SOURCE_DIR}/include/server
    ${PROJECT_SOURCE_DIR}/include/utils
    ${PROJECT_SOURCE_DIR}/include/config
     ${PROJECT_SOURCE_DIR}/include/auth
)

# 客户端包含目录
target_include_directories(CloudDiskClient PRIVATE
    ${PROJECT_SOURCE_DIR}/include/common
    ${PROJECT_SOURCE_DIR}/include/client
    ${PROJECT_SOURCE_DIR}/include/utils
    ${PROJECT_SOURCE_DIR}/include/config
    ${PROJECT_SOURCE_DIR}/include/auth
)

# 查找线程库
find_package(Threads REQUIRED)


# 服务器链接库
target_link_libraries(CloudDiskServer PRIVATE 
    Threads::Threads
    crypt
)

# 客户端链接库
target_link_libraries(CloudDiskClient PRIVATE
    crypt
)

# 安装规则(可选)
install(TARGETS CloudDiskServer CloudDiskClient
    RUNTIME DESTINATION bin
)

# 打印信息
message(STATUS "Building CloudDisk Project")
message(STATUS "  Server sources: ${SERVER_SRCS}")
message(STATUS "  Client sources: ${CLIENT_SRCS}")
