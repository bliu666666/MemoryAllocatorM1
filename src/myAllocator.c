#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <bits/mman-linux.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

#define MAX_BLOCK_CLASSES 10   // The maximum number of block types supported, for example: 8B, 16B, ..., 4096B
#define PAGE_SIZE 4096         // Assume the system page size is 4096 bytes
#define THREAD_CACHE_MAX_BLOCKS 64 // Maximum number of blocks cached per thread

typedef struct thread_cache {
    struct block* free_list[MAX_BLOCK_CLASSES];// Free list for each block size
    size_t block_count[MAX_BLOCK_CLASSES];// Block count for each block size
}thread_cache_t;

// Define a block structure
typedef struct block {
    size_t size; //Block size
    struct block* next;  // Points to the next free block
} block_t;

//Thread local cache
__thread thread_cache_t thread_cache = {{NULL}, {0}};

// Each block size (allocated in powers of 2)
static const size_t block_sizes[MAX_BLOCK_CLASSES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

// Global free list
static block_t *global_free_list[MAX_BLOCK_CLASSES] = {NULL};

// Find the corresponding block type index according to the size
static int get_block_class(size_t size) {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        if (size <= block_sizes[i]) {
            return i;
        }
    }
    return -1;
}

// Allocate new memory blocks from the global pool
static block_t *allocate_global_block(int class_index) {
    size_t block_size = block_sizes[class_index] + sizeof(block_t); // The total size of the block memory (block header + data area)

    void* ptr = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    block_t *block = (block_t*)ptr;
    block->size = block_sizes[class_index];
    block->next = NULL;
    return block;
}

// Allocating memory from the thread cache
static void* allocate_from_thread_cache(int class_index) {
    // Check if there is a block in the free list of the thread local cache
    if (thread_cache.free_list[class_index] != NULL) {
        block_t *block = thread_cache.free_list[class_index]; // Get the block at the head of the linked list
        thread_cache.free_list[class_index] = block->next; // Move the linked list head pointer to the next block (take out the head block)
        thread_cache.block_count[class_index]--; // Update the block count of the thread cache
        return (void*)((char*)block + sizeof(block_t)); // Return the starting address of the data area (skip the block header)
    }
    return NULL; // No free blocks in thread cache
}

// Reclaim to thread cache
static int cache_block_to_thread(int class_index, block_t *block) {
    // Check if the thread local cache is full
    if (thread_cache.block_count[class_index] < THREAD_CACHE_MAX_BLOCKS) {
        block->next = thread_cache.free_list[class_index]; // Insert the released block into the head of the thread cache linked list
        thread_cache.free_list[class_index] = block; // Set the new block as the new head of the linked list
        thread_cache.block_count[class_index]++; // Update thread cache block count
        return 1; // Successfully recovered to the thread cache
    }
    return 0; // Thread cache is full
}

void* my_malloc(size_t size) {
    if (size == 0) {
        return NULL; // Unable to allocate 0 bytes
    }

    int class_index = get_block_class(size);
    if (class_index == -1) {
        // If the supported block size is exceeded, call mmap directly to allocate
        size_t total_size = size + sizeof(block_t);
        void* ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap failed");
            return NULL;
        }

        // Setup block header for the oversized block
        block_t *block = (block_t *)ptr;
        block->size = size; // Store the requested size
        block->next = NULL; // Oversized blocks are not part of a list

        // Return the data pointer after the block header
        return (void *)((char *)ptr + sizeof(block_t));
    }

    // Try allocating from the thread cache
    void *ptr = allocate_from_thread_cache(class_index);
    if (ptr != NULL) {
        return ptr;
    }

    // Try allocating from the global pool
    block_t *global_block = global_free_list[class_index];
    if (global_block != NULL) {
        global_free_list[class_index] = global_block->next;
        return (void*)((char*)global_block + sizeof(block_t));
    }

    // Allocate new blocks from the system
    block_t *new_block = allocate_global_block(class_index);
    return (void*)((char*)new_block + sizeof(block_t));
}

void my_free(void* ptr) {
    if (ptr == NULL) return;

    block_t *block = (block_t*)((char*)ptr - sizeof(block_t));

    // If the block size is out of range, call munmap to free the memory.
    if (block->size > block_sizes[MAX_BLOCK_CLASSES - 1]) {
        munmap(ptr, block->size + sizeof(block_t));
        return;
    }

    int class_index = get_block_class(block->size);

    // Prioritize recovery to thread cache
    if (cache_block_to_thread(class_index, block)) {
        return;
    }

    // If the thread cache is full, recycle to the global pool
    block->next = global_free_list[class_index];
    global_free_list[class_index] = block;
}