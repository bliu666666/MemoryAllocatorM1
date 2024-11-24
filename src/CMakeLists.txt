
add_library(myAllocator myAllocator.c)
add_library(perf_cmp perf_cmp.c)

add_executable(main main.c)

target_link_libraries(main myAllocator perf_cmp)