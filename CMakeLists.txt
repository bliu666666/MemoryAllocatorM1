cmake_minimum_required(VERSION 3.22)
project(MemoryAllocatorM1 C)

set(CMAKE_C_STANDARD 11)

# Add compile options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")

# Manually specify cmocka library and include paths
include_directories(/usr/include)
link_directories(/usr/lib/x86_64-linux-gnu)

# Contains src and test directories
add_subdirectory(src)
add_subdirectory(test)