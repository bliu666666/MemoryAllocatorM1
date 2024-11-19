# Create a test executable testAllocator
add_executable(testAllocator test.c)

# Link myAllocator library, cmocka library and pthread library into the test executable
target_link_libraries(testAllocator myAllocator cmocka pthread)

# Enable testing and define test goals
enable_testing()
add_test(NAME MyAllocatorTest COMMAND testAllocator)