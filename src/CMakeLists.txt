# 创建 myAllocator 库
add_library(myAllocator myAllocator.c)
add_library(perf_cmp perf_cmp.c)

# 创建主程序可执行文件 main
add_executable(main main.c)

# 链接 myAllocator 库到 main 可执行文件
target_link_libraries(main myAllocator perf_cmp)