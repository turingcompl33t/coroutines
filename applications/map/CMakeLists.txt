# applications/map/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-map CXX)

set(BENCHMARK_ENABLE_TESTING OFF)

include("../../cmake/threads.cmake")
include("../../cmake/warnings.cmake")
include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)
add_subdirectory("../../deps/catch2" ${CMAKE_CURRENT_BINARY_DIR}/catch2)
add_subdirectory("../../deps/benchmark" ${CMAKE_CURRENT_BINARY_DIR}/benchmark)

add_executable(test "test.cpp")
target_link_libraries(test PRIVATE coro_config libcoro stdcoro Catch2 warnings)

add_executable(stats "stats.cpp")
target_link_libraries(stats PRIVATE coro_config libcoro stdcoro warnings)

add_executable(stress_sequential "stress_sequential.cpp")
target_link_libraries(stress_sequential PRIVATE coro_config libcoro stdcoro warnings)

add_executable(stress_interleaved "stress_interleaved.cpp")
target_link_libraries(stress_interleaved PRIVATE coro_config libcoro stdcoro warnings)

add_executable(bench_sequential "bench_sequential.cpp")
target_link_libraries(bench_sequential PRIVATE benchmark coro_config libcoro stdcoro warnings threads)

add_executable(bench_interleaved "bench_interleaved.cpp")
target_link_libraries(bench_interleaved PRIVATE benchmark coro_config libcoro stdcoro warnings threads)