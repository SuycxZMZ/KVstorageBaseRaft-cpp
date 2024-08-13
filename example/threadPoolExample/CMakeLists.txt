set(THREADPOOL_EXAMPLE_LISTS testThreadPool.cpp)
add_executable(testThreadPool ${THREADPOOL_EXAMPLE_LISTS})
target_link_libraries(testThreadPool pthread)
