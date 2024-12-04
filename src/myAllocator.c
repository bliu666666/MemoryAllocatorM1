#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_BLOCK_CLASSES 10    // Maximum number of block types
#define PAGE_SIZE 4096          // Assume the system page size is 4096 bytes
#define ALIGNMENT 16            // Memory alignment bytes
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) // Align Size
#define ARENA_SIZE (PAGE_SIZE * 16) // The size of each Arena can be adjusted as needed
#define THREAD_CACHE_MAX_BLOCKS 64 // Maximum number of blocks in thread cache for each block size

// Memory block structure
typedef struct block {
    size_t size;             // Block size
    struct block* next;      // Next Block
    struct block* prev;      // Previous block
    int free;                // number 1 means free, 0 means allocated
} block_t;

// Arena structure, each thread has one or more Arena
typedef struct arena {
    pthread_mutex_t lock;             // Locks to protect Arena
    block_t* free_list[MAX_BLOCK_CLASSES + 1]; // Free lists sorted by block size, adding a very large block category
    void* memory;                     // Memory area managed by Arena
    size_t size;                      // Arena Size
    struct arena* next;               // Next Arena (for supporting multiple Arenas)
} arena_t;

// Thread Cache Structure
typedef struct thread_cache {
    block_t* free_list[MAX_BLOCK_CLASSES]; // Free lists for each block size
    size_t block_count[MAX_BLOCK_CLASSES]; // Block count for each block size
} thread_cache_t;

// Global Arena list, used to manage all Arenas
static arena_t* global_arena_list = NULL;
static pthread_mutex_t global_arena_lock = PTHREAD_MUTEX_INITIALIZER;

// Defining block size classes
static const size_t block_sizes[MAX_BLOCK_CLASSES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

// Thread local variables, pointing to the Arena and thread cache of the thread
__thread arena_t* thread_arena = NULL;
__thread thread_cache_t thread_cache = {{NULL}, {0}};

// Get block category index
static int get_block_class(size_t size) {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        if (size <= block_sizes[i]) {
            return i;
        }
    }
    return MAX_BLOCK_CLASSES; // Extra large block category
}

// Initialize the thread cache
void init_thread_cache() {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        thread_cache.free_list[i] = NULL;
        thread_cache.block_count[i] = 0;
    }
}

// Initialize Arena
arena_t* create_arena() {
    arena_t* arena = (arena_t*)mmap(NULL, sizeof(arena_t), PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena == MAP_FAILED) {
        perror("Failed to allocate memory for arena");
        return NULL;
    }

    arena->memory = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena->memory == MAP_FAILED) {
        perror("Failed to allocate memory for arena space");
        munmap(arena, sizeof(arena_t));
        return NULL;
    }

    arena->size = ARENA_SIZE;
    pthread_mutex_init(&arena->lock, NULL);
    memset(arena->free_list, 0, sizeof(arena->free_list));
    arena->next = NULL;

    // Initialize the first block as the initial free block of the entire Arena
    block_t* initial_block = (block_t*)arena->memory;
    initial_block->size = arena->size - sizeof(block_t);
    initial_block->next = NULL;
    initial_block->prev = NULL;
    initial_block->free = 1;

    int class_index = get_block_class(initial_block->size);
    if (class_index >= MAX_BLOCK_CLASSES + 1) {
        class_index = MAX_BLOCK_CLASSES; // Extra large block category
    }
    arena->free_list[class_index] = initial_block;

    return arena;
}

// Get the thread's Arena and create it if it does not exist
arena_t* get_thread_arena() {
    if (thread_arena == NULL) {
        // Lock to prevent multiple threads from creating Arena at the same time
        pthread_mutex_lock(&global_arena_lock);
        thread_arena = create_arena();
        if (thread_arena == NULL) {
            pthread_mutex_unlock(&global_arena_lock);
            return NULL;
        }
        // Add the new Arena to the global Arena list
        thread_arena->next = global_arena_list;
        global_arena_list = thread_arena;
        pthread_mutex_unlock(&global_arena_lock);
    }
    return thread_arena;
}

// Add to free list (sorted by block size)
void add_to_free_list(arena_t* arena, block_t* block) {
    int class_index = get_block_class(block->size);
    if (class_index > MAX_BLOCK_CLASSES) {
        class_index = MAX_BLOCK_CLASSES;
    }
    block_t** head = &arena->free_list[class_index];
    block->next = NULL;
    block->prev = NULL;

    if (*head == NULL) {
        *head = block;
        block->free = 1;
        return;
    }

    block_t* current = *head;
    block_t* prev = NULL;
    while (current && current->size < block->size) {
        prev = current;
        current = current->next;
    }

    block->next = current;
    block->prev = prev;
    if (current) {
        current->prev = block;
    }
    if (prev) {
        prev->next = block;
    } else {
        *head = block;
    }

    block->free = 1;
}

// Remove a block from the free list
void remove_from_free_list(arena_t* arena, block_t* block) {
    int class_index = get_block_class(block->size);
    if (class_index > MAX_BLOCK_CLASSES) {
        class_index = MAX_BLOCK_CLASSES;
    }
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        arena->free_list[class_index] = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
    block->free = 0;
}

// Merge adjacent free blocks
void coalesce_blocks(arena_t* arena, block_t* block) {
    // Try to merge the next block
    block_t* next_block = (block_t*)((char*)block + block->size + sizeof(block_t));
    if ((char*)next_block < (char*)arena->memory + arena->size && next_block->free) {
        remove_from_free_list(arena, next_block);
        block->size += sizeof(block_t) + next_block->size;
    }

    // Try to merge the previous block
    block_t* prev_block = NULL;
    block_t* current = (block_t*)arena->memory;
    while ((char*)current < (char*)block) {
        prev_block = current;
        current = (block_t*)((char*)current + current->size + sizeof(block_t));
    }
    if (prev_block && prev_block->free) {
        remove_from_free_list(arena, prev_block);
        prev_block->size += sizeof(block_t) + block->size;
        block = prev_block;
    }

    add_to_free_list(arena, block);
}

// Find the best fit block
block_t* find_best_fit(arena_t* arena, size_t size, int class_index) {
    block_t* best_fit = NULL;
    // Start from the requested category, iterate through all larger categories
    for (int i = class_index; i <= MAX_BLOCK_CLASSES; i++) {
        block_t* current = arena->free_list[i];
        while (current) {
            if (current->size >= size) {
                best_fit = current;
                return best_fit; // Find the appropriate block and return immediately
            }
            current = current->next;
        }
    }
    return NULL; // No suitable block found
}

// Allocate memory from the thread cache
static void* allocate_from_thread_cache(int class_index) {
    if (thread_cache.free_list[class_index] != NULL) {
        block_t* block = thread_cache.free_list[class_index];
        thread_cache.free_list[class_index] = block->next;
        thread_cache.block_count[class_index]--;
        block->free = 0; // Set to allocated
        return (void*)((char*)block + sizeof(block_t));
    }
    return NULL;
}

// Reclaim memory blocks to thread cache
static int cache_block_to_thread(int class_index, block_t* block) {
    if (thread_cache.block_count[class_index] < THREAD_CACHE_MAX_BLOCKS) {
        block->next = thread_cache.free_list[class_index];
        thread_cache.free_list[class_index] = block;
        thread_cache.block_count[class_index]++;
        block->free = 1; // Set to free
        return 1;
    }
    return 0;
}

// Memory allocation functions
void* my_malloc(size_t size) {
    if (size == 0) {
        return NULL; // Unable to allocate 0 bytes
    }

    size = ALIGN(size); // 对齐大小
    int class_index = get_block_class(size);

    arena_t* arena = get_thread_arena();
    if (!arena) {
        return NULL;
    }

    // Prioritize allocation from thread cache
    void* ptr = allocate_from_thread_cache(class_index);
    if (ptr != NULL) {
        return ptr;
    }

    pthread_mutex_lock(&arena->lock);

    block_t* block = find_best_fit(arena, size, class_index);
    if (block) {
        remove_from_free_list(arena, block);
    } else {
        // If no suitable block is found, no new memory can be allocated
        pthread_mutex_unlock(&arena->lock);
        return NULL;
    }

    // If the block is much larger than the requested size, split the block
    if (block->size > size + sizeof(block_t) + ALIGNMENT) {
        block_t* new_block = (block_t*)((char*)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->next = NULL;
        new_block->prev = NULL;
        new_block->free = 1;

        block->size = size;

        add_to_free_list(arena, new_block);
    }

    block->free = 0;
    pthread_mutex_unlock(&arena->lock);
    return (void*)((char*)block + sizeof(block_t));
}

// Memory release function
void my_free(void* ptr) {
    if (!ptr) return;

    arena_t* arena = get_thread_arena();
    if (!arena) {
        return;
    }

    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    int class_index = get_block_class(block->size);

    // Prioritize recovery to thread cache
    if (cache_block_to_thread(class_index, block)) {
        return;
    }

    pthread_mutex_lock(&arena->lock);

    block->free = 1;

    coalesce_blocks(arena, block);

    pthread_mutex_unlock(&arena->lock);
}

// Check for memory leaks
void check_memory_leaks() {
    pthread_mutex_lock(&global_arena_lock);

    arena_t* arena = global_arena_list;
    while (arena) {
        pthread_mutex_lock(&arena->lock);

        block_t* current = (block_t*)arena->memory;
        while ((char*)current < (char*)arena->memory + arena->size) {
            if (!current->free) {
                fprintf(stderr, "Memory leak detected at %p, size: %zu\n",
                        (void*)((char*)current + sizeof(block_t)), current->size);
            }
            current = (block_t*)((char*)current + current->size + sizeof(block_t));
        }

        pthread_mutex_unlock(&arena->lock);
        arena = arena->next;
    }

    pthread_mutex_unlock(&global_arena_lock);
}