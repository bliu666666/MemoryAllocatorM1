#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myAllocator.c"

#define NUM_ALLOCATIONS 100000  // Set the number of allocations for the test
#define ALLOCATION_SIZE 1024    // The size of memory allocated each time (1KB)

// Calculate time difference
double calculate_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Testing the performance of my_malloc/my_free
void test_my_allocator_performance() {
    struct timespec start, end;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        void* ptr = my_malloc(ALLOCATION_SIZE);
        if (ptr == NULL) {
            fprintf(stderr, "my_malloc failed on iteration %d\n", i);
            exit(1);
        }
        my_free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("my_malloc/my_free: %d allocations of %d bytes took %f seconds\n", NUM_ALLOCATIONS, ALLOCATION_SIZE, time_spent);
}

// Testing the performance of malloc/free
void test_system_allocator_performance() {
    struct timespec start, end;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        void* ptr = malloc(ALLOCATION_SIZE);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed on iteration %d\n", i);
            exit(1);
        }
        free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("malloc/free: %d allocations of %d bytes took %f seconds\n", NUM_ALLOCATIONS, ALLOCATION_SIZE, time_spent);
}
