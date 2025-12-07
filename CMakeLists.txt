cmake_minimum_required(VERSION 3.26)
project(CloudDisk C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)


set(CLIENT_SRC "src/client")
set(SERVER_SRC "src/server")
set(CLIENT_SRCS "${CLIENT_SRC}/main.c")
set(SERVER_SRCS "${SERVER_SRC}/main.c")



add_executable(CloudDiskServer ${SERVER_SRCS})
add_executable(CloudDiskClient ${CLIENT_SRCS})

target_include_directories(CloudDiskServer PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(CloudDiskClient PRIVATE ${PROJECT_SOURCE_DIR}/include)