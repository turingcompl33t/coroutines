# applications/threadpool/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(threadpool CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(driver "driver.cpp")
target_link_libraries(driver PRIVATE coro_config libcoro stdcoro Threads::Threads)
target_compile_options(driver PRIVATE "-ggdb")