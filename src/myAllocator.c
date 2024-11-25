#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_BLOCK_CLASSES 10    // 最大块类型数
#define PAGE_SIZE 4096          // 假定系统页大小为4096字节
#define ALIGNMENT 16            // 内存对齐字节数
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) // 对齐大小
#define ARENA_SIZE (PAGE_SIZE * 16) // 每个 Arena 的大小，可根据需要调整
#define THREAD_CACHE_MAX_BLOCKS 64 // 每种块大小线程缓存的最大块数

// 内存块结构
typedef struct block {
    size_t size;             // 块大小
    struct block* next;      // 下一个块
    struct block* prev;      // 前一个块
    int free;                // 1表示空闲，0表示已分配
} block_t;

// Arena 结构，每个线程拥有一个或多个 Arena
typedef struct arena {
    pthread_mutex_t lock;             // 保护 Arena 的锁
    block_t* free_list[MAX_BLOCK_CLASSES + 1]; // 按块大小分类的空闲列表，增加一个超大块类别
    void* memory;                     // Arena 管理的内存区域
    size_t size;                      // Arena 的大小
    struct arena* next;               // 下一个 Arena（用于支持多个 Arena）
} arena_t;

// 线程缓存结构
typedef struct thread_cache {
    block_t* free_list[MAX_BLOCK_CLASSES]; // 每种块大小的空闲链表
    size_t block_count[MAX_BLOCK_CLASSES]; // 每种块大小的块计数
} thread_cache_t;

// 全局 Arena 列表，用于管理所有 Arena
static arena_t* global_arena_list = NULL;
static pthread_mutex_t global_arena_lock = PTHREAD_MUTEX_INITIALIZER;

// 定义块大小分类
static const size_t block_sizes[MAX_BLOCK_CLASSES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

// 线程本地变量，指向该线程的 Arena和线程缓存
__thread arena_t* thread_arena = NULL;
__thread thread_cache_t thread_cache = {{NULL}, {0}};

// 获取块分类索引
static int get_block_class(size_t size) {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        if (size <= block_sizes[i]) {
            return i;
        }
    }
    return MAX_BLOCK_CLASSES; // 超大块类别
}

// 初始化线程缓存
void init_thread_cache() {
    for (int i = 0; i < MAX_BLOCK_CLASSES; i++) {
        thread_cache.free_list[i] = NULL;
        thread_cache.block_count[i] = 0;
    }
}

// 初始化 Arena
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

    // 初始化第一个块，作为整个 Arena 的初始空闲块
    block_t* initial_block = (block_t*)arena->memory;
    initial_block->size = arena->size - sizeof(block_t);
    initial_block->next = NULL;
    initial_block->prev = NULL;
    initial_block->free = 1;

    int class_index = get_block_class(initial_block->size);
    if (class_index >= MAX_BLOCK_CLASSES + 1) {
        class_index = MAX_BLOCK_CLASSES; // 超大块类别
    }
    arena->free_list[class_index] = initial_block;

    return arena;
}

// 获取线程的 Arena，如果不存在则创建
arena_t* get_thread_arena() {
    if (thread_arena == NULL) {
        // 加锁，防止多个线程同时创建 Arena
        pthread_mutex_lock(&global_arena_lock);
        thread_arena = create_arena();
        if (thread_arena == NULL) {
            pthread_mutex_unlock(&global_arena_lock);
            return NULL;
        }
        // 将新的 Arena 加入全局 Arena 列表
        thread_arena->next = global_arena_list;
        global_arena_list = thread_arena;
        pthread_mutex_unlock(&global_arena_lock);
    }
    return thread_arena;
}

// 添加到空闲列表（按块大小排序）
void add_to_free_list(arena_t* arena, block_t* block) {
    int class_index = get_block_class(block->size);
    if (class_index > MAX_BLOCK_CLASSES) {
        class_index = MAX_BLOCK_CLASSES; // 超大块类别
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

// 从空闲列表移除块
void remove_from_free_list(arena_t* arena, block_t* block) {
    int class_index = get_block_class(block->size);
    if (class_index > MAX_BLOCK_CLASSES) {
        class_index = MAX_BLOCK_CLASSES; // 超大块类别
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

// 合并相邻空闲块
void coalesce_blocks(arena_t* arena, block_t* block) {
    // 尝试合并后一个块
    block_t* next_block = (block_t*)((char*)block + block->size + sizeof(block_t));
    if ((char*)next_block < (char*)arena->memory + arena->size && next_block->free) {
        remove_from_free_list(arena, next_block);
        block->size += sizeof(block_t) + next_block->size;
    }

    // 尝试合并前一个块
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

// 查找最佳适配块
block_t* find_best_fit(arena_t* arena, size_t size, int class_index) {
    block_t* best_fit = NULL;
    // 从请求的类别开始，遍历所有更大的类别
    for (int i = class_index; i <= MAX_BLOCK_CLASSES; i++) {
        block_t* current = arena->free_list[i];
        while (current) {
            if (current->size >= size) {
                best_fit = current;
                return best_fit; // 找到合适的块，立即返回
            }
            current = current->next;
        }
    }
    return NULL; // 未找到合适的块
}

// 从线程缓存分配内存
static void* allocate_from_thread_cache(int class_index) {
    if (thread_cache.free_list[class_index] != NULL) {
        block_t* block = thread_cache.free_list[class_index];
        thread_cache.free_list[class_index] = block->next;
        thread_cache.block_count[class_index]--;
        block->free = 0; // 设置为已分配
        return (void*)((char*)block + sizeof(block_t));
    }
    return NULL;
}

// 将内存块回收到线程缓存
static int cache_block_to_thread(int class_index, block_t* block) {
    if (thread_cache.block_count[class_index] < THREAD_CACHE_MAX_BLOCKS) {
        block->next = thread_cache.free_list[class_index];
        thread_cache.free_list[class_index] = block;
        thread_cache.block_count[class_index]++;
        block->free = 1; // 设置为空闲
        return 1;
    }
    return 0;
}

// 内存分配函数
void* my_malloc(size_t size) {
    if (size == 0) {
        return NULL; // 无法分配 0 字节
    }

    size = ALIGN(size); // 对齐大小
    int class_index = get_block_class(size);

    arena_t* arena = get_thread_arena();
    if (!arena) {
        return NULL;
    }

    // 优先从线程缓存分配
    void* ptr = allocate_from_thread_cache(class_index);
    if (ptr != NULL) {
        return ptr;
    }

    pthread_mutex_lock(&arena->lock);

    block_t* block = find_best_fit(arena, size, class_index);
    if (block) {
        remove_from_free_list(arena, block);
    } else {
        // 如果没有找到合适的块，无法再分配新的内存
        pthread_mutex_unlock(&arena->lock);
        return NULL;
    }

    // 如果块比请求的大小大得多，则分割块
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

// 内存释放函数
void my_free(void* ptr) {
    if (!ptr) return;

    arena_t* arena = get_thread_arena();
    if (!arena) {
        return;
    }

    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    int class_index = get_block_class(block->size);

    // 优先回收到线程缓存
    if (cache_block_to_thread(class_index, block)) {
        return;
    }

    pthread_mutex_lock(&arena->lock);

    block->free = 1;

    coalesce_blocks(arena, block);

    pthread_mutex_unlock(&arena->lock);
}

// 检查内存泄漏
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