#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/myAllocator.c"

// 测试 my_malloc 是否成功分配了内存
static void test_my_malloc(void **state) {
    size_t size = 1024;
    void *ptr = my_malloc(size);
    assert_non_null(ptr); // 检查分配是否成功
    my_free(ptr);
}

// 测试 my_free 是否成功释放了内存
static void test_my_free(void **state) {
    size_t size = 1024;
    void *ptr = my_malloc(size);
    assert_non_null(ptr); // 确认分配成功
    my_free(ptr);
    // 不需要检查 my_free 的返回值，但是可以确保没有崩溃
}

// 测试内存读写
static void test_memory_write(void **state) {
    size_t size = 1024;
    char *data = (char*) my_malloc(size);
    assert_non_null(data);

    // 写入数据并验证
    for (size_t i = 0; i < size; i++) {
        data[i] = (char) (i % 256);
    }
    for (size_t i = 0; i < size; i++) {
        assert_int_equal(data[i], (char) (i % 256));
    }

    my_free(data);
}

// 定义测试套件
int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_my_malloc),
            cmocka_unit_test(test_my_free),
            cmocka_unit_test(test_memory_write),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}