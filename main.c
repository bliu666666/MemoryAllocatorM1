#include "src/allocator.h"
#include <stdio.h>
#include <string.h>

int main() {
    // 请求分配 100 字节的内存
    size_t size = 100;
    char* buffer = (char*)my_malloc(size);

    if (buffer == NULL) {
        printf("my_malloc failed\n");
        return 1;
    }

    // 使用分配的内存
    strcpy(buffer, "666666666,ca march");
    printf("%s\n", buffer);

    // 释放内存
    my_free(buffer);

    return 0;
}
