#include "allocator.h"
#include <sys/mman.h>   
#include <unistd.h>    
#include <stdio.h>     
#include <string.h>    

// 定义一个结构体来存储块的大小信息
typedef struct block_header {
    size_t size;
} block_header_t;

void *my_malloc(size_t size)
{
    // 获取系统页面大小
    size_t page_size = sysconf(_SC_PAGESIZE);
    // 计算需要分配的内存大小，包含块头和用户请求的大小
    size_t total_size = sizeof(block_header_t) + size;
    // 向上对齐到页面大小的整数倍
    size_t alloc_size = ((total_size + page_size - 1) / page_size) * page_size;

    // 使用 mmap 分配内存
    void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // 在块头中存储分配大小
    block_header_t* header = (block_header_t*)ptr;
    header->size = alloc_size;

    // 返回指向用户数据的指针
    return (void*)((char*)ptr + sizeof(block_header_t));

}

void my_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // 计算块头的地址
    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));

    // 获取分配的大小
    size_t alloc_size = header->size;

    // 使用 munmap 释放内存
    if (munmap((void*)header, alloc_size) == -1) {
        perror("munmap failed");
    }
}
