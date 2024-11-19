#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <bits/mman-linux.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

#define MAX_BLOCK_CLASSES 10   // The maximum number of block types supported, for example: 8B, 16B, ..., 4096B
#define PAGE_SIZE 4096         // Assume the system page size is 4096 bytes

// Define a block structure
typedef struct block {
    struct block* next;  // Points to the next free block
} block_t;

// Free block linked list array, one linked list for each block size
static block_t* free_list[MAX_BLOCK_CLASSES];

// Each block size (allocated in powers of 2)
static const size_t block_sizes[MAX_BLOCK_CLASSES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

// Find the corresponding block type index according to the size
static int get_block_class(size_t size) {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        if (size <= block_sizes[i]) {
            return i;
        }
    }
    return -1;
}

// Allocate a page of memory and split it into blocks
static void allocate_new_page(int class_index) {
    size_t block_size = block_sizes[class_index];
    size_t blocks_per_page = PAGE_SIZE / block_size;

    // Allocate a page of memory using mmap
    void* page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    // Split pages into blocks and link them into free lists
    for (size_t i = 0; i < blocks_per_page; i++) {
        block_t* block = (block_t*)((char*)page + i * block_size);
        block->next = free_list[class_index];
        free_list[class_index] = block;
    }
}

void* my_malloc(size_t size) {
    if (size == 0) {
        return NULL; // Unable to allocate 0 bytes
    }

    int class_index = get_block_class(size);
    if (class_index == -1) {
        // If the supported block size is exceeded, call mmap directly to allocate
        void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap failed");
            return NULL;
        }
        return ptr;
    }

    // If the free list is empty, allocate a new page
    if (free_list[class_index] == NULL) {
        allocate_new_page(class_index);
    }

    // Take a block from the free list
    block_t* block = free_list[class_index];
    free_list[class_index] = block->next;
    return (void*)block;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;

    // Calculate which type the block belongs to (based on the block size and alignment rules)
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        size_t block_size = block_sizes[i];
        if ((size_t)ptr % block_size == 0) {
            block_t* block = (block_t*)ptr;
            block->next = free_list[i];
            free_list[i] = block;
            return;
        }
    }

    // If the block size is out of range, call munmap to free the memory.
    munmap(ptr, PAGE_SIZE);
}