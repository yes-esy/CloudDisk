cmake_minimum_required(VERSION 3.26)
project(CloudDisk C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -Wno-unused-result
)

set(CLIENT_SRC "src/client")
set(SERVER_SRC "src/server")
set(COMMON_SRC "src/common")
set(ULTILS_SRC "src/utils")
set(CLIENT_SRCS
    "${CLIENT_SRC}/main.c"
    "${COMMON_SRC}/Net.c"
    "${COMMON_SRC}/Command.c"
    "${CLIENT_SRC}/Client.c"
)
set(SERVER_SRCS
    "${SERVER_SRC}/main.c"
    "${SERVER_SRC}/Communication.c"
    "${COMMON_SRC}/Command.c"
    "${COMMON_SRC}/Net.c"
    "${ULTILS_SRC}/ThreadPool.c"
    "${ULTILS_SRC}/LinkedList.c"
)



add_executable(CloudDiskServer ${SERVER_SRCS})
add_executable(CloudDiskClient ${CLIENT_SRCS})

target_include_directories(CloudDiskServer
    PUBLIC ${PROJECT_SOURCE_DIR}/include/common
    PUBLIC ${PROJECT_SOURCE_DIR}/include/server
    PUBLIC ${PROJECT_SOURCE_DIR}/include/utils)
target_include_directories(CloudDiskClient
    PUBLIC ${PROJECT_SOURCE_DIR}/include/common
    PUBLIC ${PROJECT_SOURCE_DIR}/include/client
    PUBLIC ${PROJECT_SOURCE_DIR}/include/utils)
# target_link_libraries(CloudDiskServer LINK_PUBLIC libpthread)
find_package(Threads REQUIRED)
# 使用 Threads::Threads 替代直接写 libpthread
target_link_libraries(CloudDiskServer LINK_PUBLIC Threads::Threads)
# target_link_libraries(CloudDiskClient LINK_PUBLIC Threads::Threads)