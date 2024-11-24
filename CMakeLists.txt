cmake_minimum_required(VERSION 3.22)
project(MemoryAllocatorM1 C)

set(CMAKE_C_STANDARD 11)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")


include_directories(/usr/include)
link_directories(/usr/lib/x86_64-linux-gnu)


add_subdirectory(src)
add_subdirectory(test)