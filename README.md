# MemoryAllocatorM1
This project implements a custom memory allocator designed for efficient and optimized memory management. It includes memory alignment, memory pool (Arena), thread-local caching, block coalescing, and support for oversized memory allocations.  

***

## Table of contents
1.Compilation and Execution Instructions   
2.Implementation Choices  
3.List of optimization points  
4.Benchmark Results

***

## Compilation and Execution Instructions

### Requirements  

- **C Compiler**: GCC or Clang.  
- **Cmake:Version** 3..22 or higher with cmocka library for testing and pthread library for multithreading  
- **Linux System**  

### Steps  
Run the following command in the project root directory:  
1.**Clone the Repository**:  
    
```  
    git clone <repository_url>  
    cd <repository_url>
``` 

2.**Build the Project**:
```  
    mkdir build
    cd build
    cmake..
    make
```  

3.**Execute the Main Program**:
```
    ./main <size_in_bytes_to_allocate> <num_allocations_to_test> <min_allocation_size> <max_allocation_size>
    
    # Parameter Description：
    # size_in_bytes_to_allocate：The size of memory allocated at a time (in bytes)
    # num_allocations_to_test：The total number of allocation/deallocation operations during the performance test
    # min_allocation_size：Minimum memory block size for random allocations in performance testing
    # max_allocation_size: Maximum memory block size for random allocations in performance testing
```  

4.**Run the Tests**  
```
    cd build/test
    ctest
```  
***

## Implementation Choices  

1.**Memory Alignment**:  

- All memory allocations are aligned to 16 bytes to ensure efficient memory access and compatibility with modern processors.  

2.**Thread Caching**:

- Each thread maintains its own memory cache (thread_cache), and prioritizes allocating small blocks of memory from the cache, reducing global lock contention and improving multi-threaded performance.    

3.**Arena Mechanism**:

- Each thread independently manages its own memory area (Arena), implements memory management through the global Arena list (global_arena_list), and supports dynamic expansion.

4.**Large Block Allocation**:

- For memory blocks larger than 4096 bytes, call mmap directly to allocate them to avoid affecting cache and Arena management.  

5.**Block Coalescing**:  

- It supports merging adjacent free blocks to reduce memory fragmentation and improve memory utilization when releasing memory.  

6.**Memory leak detection**:

- After the program ends, it traverses the global_arena_list to check for unreleased memory blocks and outputs potential memory leak information.

***

## List of optimization points  

| Optimization points             | Description                                                                                                               |
|---------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| Memory Alignment                | Implement 16-byte alignment and optimize memory access efficiency.                                                        |
| Thread Cache                    | Improve multithreaded performance of small memory allocations and reduce lock contention.                                 |
| Arena Mechanism                 | Each thread manages its own memory area independently, reduce interference between threads.                               |
| Block Merging Mechanism         | Support dynamic merging of adjacent free blocks, improve memory utilization and reduce fragmentation.                     |
| Large Block Memory Optimization | For blocks larger than 4096 bytes, directly use mmap to allocate to avoid interfering with other memory management logic. |
| Memory Leak Detection | Provide a leak detection mechanism based on global_arena_list to facilitate debugging and verification of memory management. |

***

## Benchmark Results 

***