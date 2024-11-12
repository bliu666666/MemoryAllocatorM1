# 创建测试可执行文件 testAllocator
add_executable(testAllocator test.c)

# 链接 myAllocator 库和 cmocka 库到测试可执行文件
target_link_libraries(testAllocator myAllocator cmocka)

# 启用测试并定义测试目标
enable_testing()
add_test(NAME MyAllocatorTest COMMAND testAllocator)