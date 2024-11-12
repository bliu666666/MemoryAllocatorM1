#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h> // 用于 size_t 类型

void* my_malloc(size_t size);
void my_free(void* ptr);

#endif // ALLOCATOR_H
