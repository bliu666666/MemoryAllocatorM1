#include "perf_cmp.c"

int main(int argc,char **argv) {
    if (argc!=3)
    {
        fprintf(stderr,"Usage: need 3 arguments\n");
        exit(0);
    }
    size_t size=atoi(argv[1]);// Allocate argv[1]bytes of memory
    int num_allocations=atoi(argv[2]);

    printf("Allocating %zu bytes of memory...\n", size);
    void* ptr = my_malloc(size);

    if (ptr == NULL) {
        fprintf(stderr,"Failed to allocate memory.\n");
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
    test_my_allocator_performance(num_allocations,size);

    printf("Testing system allocator (malloc/free)...\n");
    test_system_allocator_performance(num_allocations,size);

    return 0;
}
