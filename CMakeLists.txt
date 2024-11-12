cmake_minimum_required(VERSION 3.22)
project(MemoryAllocatorM1 C)

set(CMAKE_C_STANDARD 11)

# 添加编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")

# 手动指定 cmocka 库和包含路径
include_directories(/usr/include)
link_directories(/usr/lib/x86_64-linux-gnu)

# 包含 src 和 test 目录
add_subdirectory(src)
add_subdirectory(test)