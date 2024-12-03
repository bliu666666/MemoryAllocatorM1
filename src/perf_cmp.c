#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myAllocator.c"
#include <pthread.h>

typedef struct{
    int num_allocations;
    size_t min_allocation_size;
    size_t max_allocation_size;
}thread_data_t;//Task data for each thread

// Calculate time difference
double calculate_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Generate random allocation size between min_size and max_size
size_t generate_random_size(size_t min_size, size_t max_size) {
    return min_size + rand() % (max_size - min_size + 1);
}

//Tasks executed by a single thread of my_malloc/my_free
void *thread_task_custom(void *arg)
{
    thread_data_t data=*(thread_data_t*)arg;
    srand(time(NULL));
    for (int i=0;i<data.num_allocations;i++)
    {
        size_t allocation_size= generate_random_size(data.min_allocation_size,data.max_allocation_size);
        void* ptr = my_malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "my_malloc failed in thread %lu at iteration %d\n", pthread_self(),i);
            pthread_exit(NULL);
        }
        my_free(ptr);
    }
    pthread_exit(NULL);
}

//Tasks executed by a single thread of malloc/free
void *thread_task_system(void *arg)
{
    thread_data_t data=*(thread_data_t*)arg;
    srand(time(NULL));
    for (int i=0;i<data.num_allocations;i++)
    {
        size_t allocation_size= generate_random_size(data.min_allocation_size,data.max_allocation_size);
        void* ptr = malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed in thread %lu at iteration %d\n", pthread_self(),i);
            pthread_exit(NULL);
        }
        free(ptr);
    }
    pthread_exit(NULL);
}

// Test the performance of my_malloc/my_free
void test_my_allocator_performance(int num_allocations,size_t min_allocation_size, size_t max_allocation_size) {
    struct timespec start, end;

    srand(time(NULL));

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_allocations; i++) {
        size_t allocation_size = generate_random_size(min_allocation_size, max_allocation_size);
        void* ptr = my_malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "my_malloc failed on iteration %d\n", i);
            exit(1);
        }
        my_free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("my_malloc/my_free: %d allocations of sizes between %zu and %zu bytes took %f seconds\n",num_allocations, min_allocation_size, max_allocation_size, time_spent);
}

//Multithreaded performance testing of my_malloc/my_free
void test_multithread_allocator_performance(int num_allocations,int num_threads,size_t min_allocation_size, size_t max_allocation_size)
{
    pthread_t threads[num_threads];
    thread_data_t datas[num_threads];

    //Set up test data for each thread
    for (int i=0;i<num_threads;i++)
    {
        datas[i].num_allocations=num_allocations/num_threads;
        datas[i].min_allocation_size=min_allocation_size;
        datas[i].max_allocation_size=max_allocation_size;
    }

    struct timespec start,end;
    //Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    //Creat threads
    for (int i=0;i<num_threads;i++)
    {
        pthread_create(&threads[i],NULL,thread_task_custom,(void *)&datas[i]);
    }

    //Wait for all threads to complete
    for (int i=0;i<num_threads;i++)
    {
        pthread_join(threads[i],NULL);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("Custom Allocator (my_malloc/my_free): %d threads, %d allocations, "
           "sizes between %zu and %zu bytes took %f seconds\n",
           num_threads,num_allocations,min_allocation_size,max_allocation_size,time_spent);
}

// Test the performance of malloc/free
void test_system_allocator_performance(int num_allocations,size_t min_allocation_size, size_t max_allocation_size) {
    struct timespec start, end;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_allocations; i++) {
        size_t allocation_size = generate_random_size(min_allocation_size, max_allocation_size);
        void* ptr = malloc(allocation_size);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed on iteration %d\n", i);
            exit(1);
        }
        free(ptr);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("malloc/free: %d allocations of sizes between %zu and %zu bytes took %f seconds\n",
           num_allocations, min_allocation_size, max_allocation_size, time_spent);
}

//Multithreaded performance testing of malloc/free
void test_multithread_system_allocator_performance(int num_allocations,int num_threads,size_t min_allocation_size, size_t max_allocation_size)
{
    pthread_t threads[num_threads];
    thread_data_t datas[num_threads];

    //Set up test data for each thread
    for (int i=0;i<num_threads;i++)
    {
        datas[i].num_allocations=num_allocations/num_threads;
        datas[i].min_allocation_size=min_allocation_size;
        datas[i].max_allocation_size=max_allocation_size;
    }

    struct timespec start,end;
    //Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

    //Creat threads
    for (int i=0;i<num_threads;i++)
    {
        pthread_create(&threads[i],NULL,thread_task_system,(void *)&datas[i]);
    }

    //Wait for all threads to complete
    for (int i=0;i<num_threads;i++)
    {
        pthread_join(threads[i],NULL);
    }

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = calculate_time(start, end);

    printf("Custom Allocator (my_malloc/my_free): %d threads, %d allocations, "
           "sizes between %zu and %zu bytes took %f seconds\n",
           num_threads,num_allocations,min_allocation_size,max_allocation_size,time_spent);
}