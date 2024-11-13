#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <bits/mman-linux.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


// 定义一个结构体来存储块的大小信息
typedef struct block_header {
    size_t size;
} block_header_t;

typedef struct free_block {
    size_t size;
    struct free_block* next;
} free_block_t;

free_block_t* free_list = NULL;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;//增加全局锁


void* my_malloc(size_t size) {
    pthread_mutex_lock(&lock); // 加锁

    size_t total_size = sizeof(size_t) + size; // 包括块头的总大小
    free_block_t* best_fit = NULL;
    free_block_t** best_fit_prev = NULL;
    free_block_t** current = &free_list;

    // 在自由链表中查找最优块
    while (*current != NULL) {
        if ((*current)->size >= total_size) {
            if (best_fit == NULL || (*current)->size < best_fit->size) {
                best_fit = *current;
                best_fit_prev = current;
            }
        }
        current = &(*current)->next;
    }

    // 如果找到了合适的块
    if (best_fit != NULL) {
        *best_fit_prev = best_fit->next; // 从链表中移除最佳块
        pthread_mutex_unlock(&lock); // 解锁
        return (void*)((char*)best_fit + sizeof(size_t));
    }

    // 如果没有找到合适的块，则使用 mmap 分配新内存
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_size = (total_size + page_size - 1) & ~(page_size - 1);

    void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        pthread_mutex_unlock(&lock); // 解锁
        return NULL;
    }

    *((size_t*)ptr) = alloc_size;
    pthread_mutex_unlock(&lock); // 解锁
    return (void*)((char*)ptr + sizeof(size_t));
}

void my_free(void* ptr) {
    if (ptr == NULL) return;
    pthread_mutex_lock(&lock); // 加锁
    
    // 获取块头信息
    size_t* block_start = (size_t*)((char*)ptr - sizeof(size_t));
    size_t alloc_size = *block_start;

    // 将释放的块插入到自由链表中
    free_block_t* new_block = (free_block_t*)block_start;
    new_block->size = alloc_size;
    new_block->next = free_list;
    free_list = new_block;

      pthread_mutex_unlock(&lock); // 解锁
}
