#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/myAllocator.c"

// Tests allocating and freeing fixed-size memory blocks
static void test_fixed_block_allocation(void **state) {
    size_t size = 32;
    void *ptr = my_malloc(size);
    assert_non_null(ptr);

    // Write test data
    char *data = (char *)ptr;
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    // Verify that the written data is correct
    for (size_t i = 0; i < size; i++) {
        assert_int_equal(data[i], (char)(i % 256));
    }

    my_free(ptr);
}

// Testing for memory allocations outside the block size range
static void test_large_block_allocation(void **state) {
    size_t size = 8192;
    void *ptr = my_malloc(size);
    assert_non_null(ptr);

    // Write test data
    char *data = (char *)ptr;
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    // Verify that the written data is correct
    for (size_t i = 0; i < size; i++) {
        assert_int_equal(data[i], (char)(i % 256));
    }

    my_free(ptr);
}

// Testing the behavior of zero-byte allocations
static void test_zero_block_allocation(void **state) {
    void *ptr = my_malloc(0);
    assert_null(ptr);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_fixed_block_allocation),
            cmocka_unit_test(test_large_block_allocation),
            cmocka_unit_test(test_zero_block_allocation),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}