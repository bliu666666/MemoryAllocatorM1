#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h> // Use uintptr_t for pointer arithmetic

#define ALIGNMENT 16 // Define the number of bytes for memory alignment (typically 16 or 8, depending on the system)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) // Calculate the aligned size

// Define the structure for the memory block header (optimized version)
// Changed size from size_t to uint32_t to reduce metadata overhead
typedef struct block_header {
    uint32_t size; // Size of the block, using uint32_t to reduce space usage
} block_header_t;

// Define the structure for a free memory block
typedef struct free_block {
    uint32_t size; // Size of the block
    struct free_block* next; // Pointer to the next free block
} free_block_t;

// Pointer to the head of the free list, pointing to the first free block
free_block_t* free_list = NULL;

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

// Custom malloc function to allocate memory of the specified size
void* my_malloc(size_t size) {
    pthread_mutex_lock(&lock); // Lock to ensure thread safety

    // Calculate the total size, including block header and alignment padding
    size_t total_size = sizeof(block_header_t) + size + (ALIGNMENT - 1);
    free_block_t* best_fit = NULL; // Pointer to store the best fit free block
    free_block_t** best_fit_prev = NULL; // Pointer to record the previous block of the best fit
    free_block_t** current = &free_list; // Pointer to traverse the free list

    // Search for a suitable memory block in the free list (best-fit strategy)
    while (*current != NULL) {
        if ((*current)->size >= total_size) {
            // Update the best-fit block if the current block is smaller and sufficient
            if (best_fit == NULL || (*current)->size < best_fit->size) {
                best_fit = *current;
                best_fit_prev = current;
            }
        }
        current = &(*current)->next; // Move to the next block
    }

    // If a suitable block is found
    if (best_fit != NULL) {
        *best_fit_prev = best_fit->next; // Remove the block from the free list
        pthread_mutex_unlock(&lock); // Unlock, operation complete
        void* user_ptr = (void*)((char*)best_fit + sizeof(block_header_t)); // Return pointer to the user data section
        add_allocated_block(user_ptr, best_fit->size); // Add to the allocated memory tracking list
        return user_ptr;
    }

    // If no suitable block is found, allocate new memory using mmap
    size_t page_size = sysconf(_SC_PAGESIZE); // Get the system page size
    size_t alloc_size = (total_size + page_size - 1) & ~(page_size - 1); // Align allocation size to the page size
    void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // Allocate memory with mmap

    if (ptr == MAP_FAILED) { // Check if mmap failed
        perror("mmap failed");
        pthread_mutex_unlock(&lock); // Unlock
        return NULL;
    }

    // Align the pointer address to meet alignment requirements
    uintptr_t raw_address = (uintptr_t)ptr + sizeof(block_header_t); // Skip the block header
    uintptr_t aligned_address = (raw_address + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); // Calculate the aligned address

    // Store block header information to record the allocation size
    block_header_t* header = (block_header_t*)(aligned_address - sizeof(block_header_t)); // Get the block header pointer
    header->size = (uint32_t)alloc_size; // Store the total size of the block

    void* user_ptr = (void*)aligned_address;
    add_allocated_block(user_ptr, (uint32_t)alloc_size); // Add to the allocated memory tracking list

    pthread_mutex_unlock(&lock); // Unlock, operation complete
    return user_ptr; // Return the aligned pointer
}

// Custom free function to release previously allocated memory
void my_free(void* ptr) {
    if (ptr == NULL) return; // If the pointer is NULL, return immediately
    pthread_mutex_lock(&lock); // Lock to ensure thread safety

    // Retrieve block header information to find the start of the block
    block_header_t* block_start = (block_header_t*)((char*)ptr - sizeof(block_header_t)); // Get the block header pointer
    uint32_t alloc_size = block_start->size; // Read the size of the block

    // Insert the released block at the head of the free list
    free_block_t* new_block = (free_block_t*)block_start; // Create a new free block structure
    new_block->size = alloc_size; // Set the size of the free block
    new_block->next = free_list; // Link it to the free list
    free_list = new_block; // Update the head pointer of the free list

    remove_allocated_block(ptr); // Remove from the allocated memory list

    pthread_mutex_unlock(&lock); // Unlock, operation complete
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
