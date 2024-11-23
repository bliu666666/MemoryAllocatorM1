#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/myAllocator.c"
#include <pthread.h>

static void* thread_test(void* arg) {
    (void)arg;
    void* ptr = my_malloc(64);
    assert_non_null(ptr);
    my_free(ptr);
    return NULL;
}

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

// Testing the independence of multithreaded Arena
static void test_thread_cache(void **state) {
    void *ptr1 = my_malloc(32);
    void *ptr2 = my_malloc(32);

    assert_non_null(ptr1);
    assert_non_null(ptr2);

    my_free(ptr1);
    my_free(ptr2);

    void *ptr3 = my_malloc(32);
    assert_ptr_equal(ptr3, ptr2); // Should be allocated from the thread cache
}

//Test thread-local Arena independence (requires multithreading)
static void test_multithread_cache(void **state) {
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_test, NULL);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_fixed_block_allocation),
            cmocka_unit_test(test_large_block_allocation),
            cmocka_unit_test(test_zero_block_allocation),
            cmocka_unit_test(test_thread_cache),
            cmocka_unit_test(test_multithread_cache),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}