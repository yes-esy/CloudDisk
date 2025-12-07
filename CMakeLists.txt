cmake_minimum_required(VERSION 3.26)
set(CMAKE_C_STANDARD 10)
set(CMAKE_C_STANDARD_REQUIRED ON)
project(CloudDisk C)
add_compile_options(-Wall -Wextra)
file(GLOB_RECURSE SRCS "${PROJECT_SOURCE_DIR}/${SRC}/*.c")


add_executable(${PROJECT_NAME} ${SRCS})

target_include_directories(${PROJECT_NAME} PRIVATE include)