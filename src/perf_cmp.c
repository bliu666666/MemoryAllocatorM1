#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myAllocator.c"

#define NUM_ALLOCATIONS 100000  // 设置测试的分配次数
#define ALLOCATION_SIZE 1024    // 每次分配的内存大小（1KB）

// 计算时间差
double calculate_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// 测试 my_malloc/my_free 的性能
void test_my_allocator_performance() {
    struct timespec start, end;

    // 开始计时
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        void* ptr = my_malloc(ALLOCATION_SIZE);
        if (ptr == NULL) {
            fprintf(stderr, "my_malloc failed on iteration %d\n", i);
            exit(1);
        }
        my_free(ptr);
    }

    // 结束计时
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("my_malloc/my_free: %d allocations of %d bytes took %f seconds\n", NUM_ALLOCATIONS, ALLOCATION_SIZE, time_spent);
}

// 测试 malloc/free 的性能
void test_system_allocator_performance() {
    struct timespec start, end;

    // 开始计时
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        void* ptr = malloc(ALLOCATION_SIZE);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed on iteration %d\n", i);
            exit(1);
        }
        free(ptr);
    }

    // 结束计时
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("malloc/free: %d allocations of %d bytes took %f seconds\n", NUM_ALLOCATIONS, ALLOCATION_SIZE, time_spent);
}
