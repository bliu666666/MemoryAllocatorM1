#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/myAllocator.c"

// Test if my_malloc successfully allocates memory
static void test_my_malloc(void **state) {
    size_t size = 1024;
    void *ptr = my_malloc(size);
    assert_non_null(ptr); // Check if allocation was successful
    my_free(ptr);
}

// Test if my_free successfully frees memory
static void test_my_free(void **state) {
    size_t size = 1024;
    void *ptr = my_malloc(size);
    assert_non_null(ptr); // Ensure allocation was successful
    my_free(ptr);
    // No need to check the return value of my_free, but ensure no crash occurs
}

// Test memory read and write operations
static void test_memory_write(void **state) {
    size_t size = 1024;
    char *data = (char*) my_malloc(size);
    assert_non_null(data);

    // Write data and verify
    for (size_t i = 0; i < size; i++) {
        data[i] = (char) (i % 256);
    }
    for (size_t i = 0; i < size; i++) {
        assert_int_equal(data[i], (char) (i % 256));
    }

    my_free(data);
}

// Define the test suite
int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_my_malloc),
            cmocka_unit_test(test_my_free),
            cmocka_unit_test(test_memory_write),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
