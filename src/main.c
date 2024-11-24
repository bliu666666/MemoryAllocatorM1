#include "perf_cmp.c" // Header file containing performance comparison functions
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <size in KB>\n", argv[0]);
        exit(0);
    }

    size_t size = atoi(argv[1]) * 1024; // Interpret argv[1] as KB and convert it to bytes
    void* ptr = my_malloc(size);

    if (ptr == NULL) {
        printf("Failed to allocate memory.\n");
        return 1;
    }

    printf("Memory allocated at address %p\n", ptr);

    // Write and read data
    char* data = (char*)ptr;
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    printf("Memory write completed.\n");

    // Free the allocated memory
    my_free(ptr);
    printf("Memory freed successfully.\n");

    // Detect memory leaks
    check_memory_leaks();

    // Test the performance of different allocators
    printf("Testing custom allocator (my_malloc/my_free)...\n");
    test_my_allocator_performance();

    printf("Testing system allocator (malloc/free)...\n");
    test_system_allocator_performance();

    return 0;
}
