#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_BLOCK_CLASSES 10   // The maximum number of block types supported, for example: 8B, 16B, ..., 4096B
#define PAGE_SIZE 4096         // Assume the system page size is 4096 bytes
#define THREAD_CACHE_MAX_BLOCKS 64 // Maximum number of blocks cached per thread
#define ALIGNMENT 16 // Define the number of bytes for memory alignment (typically 16 or 8, depending on the system)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) // Calculate the aligned size

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
// Global mutex lock to ensure thread-safe memory allocation
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Define the structure for tracking allocated memory
typedef struct allocated_block {
    void* ptr; // Pointer to the allocated memory
    uint32_t size; // Size of the allocated memory
    struct allocated_block* next; // Pointer to the next allocated block
} allocated_block_t;

// Pointer to the head of the allocated memory list
allocated_block_t* allocated_list = NULL;

// Find the corresponding block type index according to the size
static int get_block_class(size_t size) {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        if (size <= block_sizes[i]) {
            return i;
        }
    }
    return -1;
}

// Function to add an allocated block record
void add_allocated_block(void* ptr, uint32_t size) {
    allocated_block_t* new_block = (allocated_block_t*)malloc(sizeof(allocated_block_t));
    if (new_block == NULL) {
        perror("Failed to allocate memory for tracking");
        return;
    }
    new_block->ptr = ptr;
    new_block->size = size;
    new_block->next = allocated_list;
    allocated_list = new_block;
}

// Function to remove an allocated block record
void remove_allocated_block(void* ptr) {
    allocated_block_t** current = &allocated_list;
    while (*current != NULL) {
        if ((*current)->ptr == ptr) {
            allocated_block_t* to_remove = *current;
            *current = (*current)->next;
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
    fprintf(stderr, "Warning: Attempt to free untracked memory at %p\n", ptr);
}

// Allocate oversized blocks
static void *allocate_large_block(size_t size) {
    size_t total_size = ALIGN(size + sizeof(block_t));
    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    block_t *block = (block_t *)ptr;
    block->size = size;
    add_allocated_block(ptr, total_size);
    return (void *)((char *)ptr + sizeof(block_t));
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
    pthread_mutex_lock(&lock);
    if (size == 0) {
        pthread_mutex_unlock(&lock);
        return NULL; // Unable to allocate 0 bytes
    }

    size = ALIGN(size); // Alignment size
    int class_index = get_block_class(size);
    if (class_index == -1) {
        // Handle oversized allocations using allocate_large_block
        void *result = allocate_large_block(size);
        if (result) {
            add_allocated_block(result, size); // Track the allocation
        }
        pthread_mutex_unlock(&lock);
        return result;
    }

    // Try allocating from the thread cache
    void *ptr = allocate_from_thread_cache(class_index);
    if (!ptr) {
        // If thread cache is empty, check the global free list
        block_t *block = global_free_list[class_index];
        if (block) {
            global_free_list[class_index] = block->next; // Remove the block from global free list
            ptr = (void *)((char *)block + sizeof(block_t));
        } else {
            // Allocate a new block if no free blocks are available
            size_t total_size = ALIGN(block_sizes[class_index] + sizeof(block_t));
            block_t *new_block = (block_t *)allocate_large_block(total_size);
            if (new_block) {
                new_block->size = block_sizes[class_index];
                ptr = (void *)((char *)new_block + sizeof(block_t));
            }
        }
    }

    if (ptr) {
        add_allocated_block(ptr, size); // Track the allocation
    }

    pthread_mutex_unlock(&lock);
    return ptr;
}

void my_free(void* ptr) {
    if (ptr == NULL) return;

    pthread_mutex_lock(&lock);

    block_t *block = (block_t*)((char*)ptr - sizeof(block_t));


    int class_index = get_block_class(block->size);

    if (class_index == -1) {
        // Handle oversized block
        remove_allocated_block(ptr);
        munmap(block, block->size + sizeof(block_t));
    } else if (!cache_block_to_thread(class_index, block)) { // Try to cache the block in the thread cache
        // If thread cache is full, add the block back to the global free list
        block->next = global_free_list[class_index];
        global_free_list[class_index] = block;
    }

    pthread_mutex_unlock(&lock);
}

// Function to check for memory leaks
void check_memory_leaks() {
    pthread_mutex_lock(&lock); // Lock to ensure thread safety

    allocated_block_t* current = allocated_list;
    while (current != NULL) {
        fprintf(stderr, "Memory leak detected: pointer %p of size %u bytes\n", current->ptr, current->size);
        current = current->next;
    }

    pthread_mutex_unlock(&lock); // Unlock, operation complete
}

