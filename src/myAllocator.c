#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h> // 使用 uintptr_t 以便进行指针算术

#define ALIGNMENT 16 // 定义内存对齐字节数（一般为 16 或 8，根据系统需求）
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) // 计算对齐后的大小

// 定义内存块头部结构，优化后的版本
// 将 size 从 size_t 改为 uint32_t，以减少元数据占用内存
typedef struct block_header {
    uint32_t size; // 块的大小，使用 uint32_t 以减少占用空间
} block_header_t;

// 定义空闲块结构
typedef struct free_block {
    uint32_t size; // 块的大小
    struct free_block* next; // 指向下一个空闲块
} free_block_t;

// 空闲链表的头指针，指向第一个空闲块
free_block_t* free_list = NULL;

// 全局互斥锁，用于确保内存分配线程安全
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// 定义跟踪已分配内存的结构
typedef struct allocated_block {
    void* ptr; // 指向已分配内存的指针
    uint32_t size; // 分配的内存大小
    struct allocated_block* next; // 指向下一个已分配块
} allocated_block_t;

// 已分配内存链表的头指针
allocated_block_t* allocated_list = NULL;

// 添加分配记录函数
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

// 删除分配记录函数
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

// 自定义 malloc 函数，分配指定大小的内存
void* my_malloc(size_t size) {
    pthread_mutex_lock(&lock); // 加锁，确保线程安全

    // 计算总大小，包括块头和对齐所需的额外空间
    size_t total_size = sizeof(block_header_t) + size + (ALIGNMENT - 1);
    free_block_t* best_fit = NULL; // 用于存储找到的最优空闲块
    free_block_t** best_fit_prev = NULL; // 用于记录最优块的前一个指针
    free_block_t** current = &free_list; // 用于遍历空闲链表

    // 在空闲链表中查找合适的内存块（最佳适配）
    while (*current != NULL) {
        if ((*current)->size >= total_size) {
            // 如果当前块满足要求，且比之前找到的最优块更小，则更新最优块
            if (best_fit == NULL || (*current)->size < best_fit->size) {
                best_fit = *current;
                best_fit_prev = current;
            }
        }
        current = &(*current)->next; // 继续遍历链表
    }

    // 如果找到了合适的空闲块
    if (best_fit != NULL) {
        *best_fit_prev = best_fit->next; // 从空闲链表中移除该块
        pthread_mutex_unlock(&lock); // 解锁，操作完成
        void* user_ptr = (void*)((char*)best_fit + sizeof(block_header_t)); // 返回指向用户数据部分的指针
        add_allocated_block(user_ptr, best_fit->size); // 添加到分配跟踪列表
        return user_ptr;
    }

    // 如果没有找到合适的块，使用 mmap 分配新的内存
    size_t page_size = sysconf(_SC_PAGESIZE); // 获取系统页大小
    size_t alloc_size = (total_size + page_size - 1) & ~(page_size - 1); // 计算按页大小对齐的分配大小
    void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 使用 mmap 分配内存

    if (ptr == MAP_FAILED) { // 如果 mmap 失败，返回 NULL
        perror("mmap failed");
        pthread_mutex_unlock(&lock); // 解锁
        return NULL;
    }

    // 对齐指针地址，使其满足对齐要求
    uintptr_t raw_address = (uintptr_t)ptr + sizeof(block_header_t); // 跳过块头部分
    uintptr_t aligned_address = (raw_address + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); // 计算对齐后的地址

    // 存储块头信息，以记录分配大小
    block_header_t* header = (block_header_t*)(aligned_address - sizeof(block_header_t)); // 获取块头指针
    header->size = (uint32_t)alloc_size; // 存储块的总大小

    void* user_ptr = (void*)aligned_address;
    add_allocated_block(user_ptr, (uint32_t)alloc_size); // 添加到分配跟踪列表

    pthread_mutex_unlock(&lock); // 解锁，操作完成
    return user_ptr; // 返回对齐后的指针
}

// 自定义 free 函数，释放之前分配的内存
void my_free(void* ptr) {
    if (ptr == NULL) return; // 如果传入的指针为 NULL，直接返回
    pthread_mutex_lock(&lock); // 加锁，确保线程安全

    // 获取块头信息，以找回块的起始地址
    block_header_t* block_start = (block_header_t*)((char*)ptr - sizeof(block_header_t)); // 获取块头指针
    uint32_t alloc_size = block_start->size; // 读取块的大小

    // 将释放的块插入到空闲链表的头部
    free_block_t* new_block = (free_block_t*)block_start; // 创建新的空闲块结构
    new_block->size = alloc_size; // 设置空闲块的大小
    new_block->next = free_list; // 将其链接到空闲链表中
    free_list = new_block; // 更新空闲链表的头指针

    remove_allocated_block(ptr); // 从已分配列表中移除

    pthread_mutex_unlock(&lock); // 解锁，操作完成
}

// 检测内存泄漏的函数
void check_memory_leaks() {
    pthread_mutex_lock(&lock); // 加锁，确保线程安全

    allocated_block_t* current = allocated_list;
    while (current != NULL) {
        fprintf(stderr, "Memory leak detected: pointer %p of size %u bytes\n", current->ptr, current->size);
        current = current->next;
    }

    pthread_mutex_unlock(&lock); // 解锁，操作完成
}

