# Create myAllocator library and perf_cmp library
add_library(myAllocator myAllocator.c)
add_library(perf_cmp perf_cmp.c)

# Create the main program executable file main
add_executable(main main.c)

# Link the myAllocator library and the perf_cmp library into the main executable
target_link_libraries(main myAllocator perf_cmp)