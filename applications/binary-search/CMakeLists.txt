# applications/binary-search/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-binary-search CXX)

include("../../cmake/threads.cmake")
include("../../cmake/warnings.cmake")
include("../../cmake/benchmark.cmake")
include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)
add_subdirectory("../../deps/benchmark" ${CMAKE_CURRENT_BINARY_DIR}/benchmark)

add_executable(bench "bench.cpp")
target_link_libraries(bench PRIVATE coro_config libcoro stdcoro benchmark warnings threads)