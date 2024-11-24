
add_executable(testAllocator test.c)

target_link_libraries(testAllocator myAllocator cmocka)


enable_testing()
add_test(NAME MyAllocatorTest COMMAND testAllocator)