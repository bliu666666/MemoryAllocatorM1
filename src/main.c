#include "perf_cmp.c" // 包含性能比较相关函数的头文件
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <size in KB>\n", argv[0]);
        exit(0);
    }

    size_t size = atoi(argv[1]) * 1024; // 将 argv[1] 解释为 KB，并转换为字节
    void* ptr = my_malloc(size);

    if (ptr == NULL) {
        printf("Failed to allocate memory.\n");
        return 1;
    }

    printf("Memory allocated at address %p\n", ptr);

    // 写入和读取数据
    char* data = (char*)ptr;
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    printf("Memory write completed.\n");

    // 释放分配的内存
    my_free(ptr);
    printf("Memory freed successfully.\n");

    // 检测内存泄漏
    check_memory_leaks();

    // 测试不同分配器性能
    printf("Testing custom allocator (my_malloc/my_free)...\n");
    test_my_allocator_performance();

    printf("Testing system allocator (malloc/free)...\n");
    test_system_allocator_performance();

    return 0;
}

/*
详细注释：
1. `main` 函数用于测试自定义分配器的功能与性能。
2. 在运行程序时，需传入内存分配大小（以 KB 为单位），并转换为字节用于分配内存。
3. 调用 `my_malloc` 函数分配内存，并检查是否成功。
4. 成功后，模拟数据写入操作以测试分配内存的可用性。
5. 调用 `my_free` 函数释放分配的内存，确保正确的内存管理。
6. 使用 `check_memory_leaks` 检查内存泄漏情况，以确保所有分配的内存都已释放。
7. 测试自定义内存分配器和系统分配器的性能，以比较它们的效率。
*/
