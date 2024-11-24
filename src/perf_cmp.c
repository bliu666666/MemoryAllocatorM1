#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myAllocator.c"

// Calculate time difference
double calculate_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Generate random allocation size between min_size and max_size
size_t generate_random_size(size_t min_size, size_t max_size) {
    return min_size + rand() % (max_size - min_size + 1);
}

// Test the performance of my_malloc/my_free
void test_my_allocator_performance(int num_allocations,size_t min_allocation_size, size_t max_allocation_size) {
    struct timespec start, end;

    srand(time(NULL));

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_allocations; i++) {
        size_t allocation_size = generate_random_size(min_allocation_size, max_allocation_size);
        void* ptr = my_malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "my_malloc failed on iteration %d\n", i);
            exit(1);
        }
        my_free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("my_malloc/my_free: %d allocations of sizes between %zu and %zu bytes took %f seconds\n",num_allocations, min_allocation_size, max_allocation_size, time_spent);
}

// Test the performance of malloc/free
void test_system_allocator_performance(int num_allocations,size_t min_allocation_size, size_t max_allocation_size) {
    struct timespec start, end;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_allocations; i++) {
        size_t allocation_size = generate_random_size(min_allocation_size, max_allocation_size);
        void* ptr = malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed on iteration %d\n", i);
            exit(1);
        }
        free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("malloc/free: %d allocations of sizes between %zu and %zu bytes took %f seconds\n",
           num_allocations, min_allocation_size, max_allocation_size, time_spent);
}
