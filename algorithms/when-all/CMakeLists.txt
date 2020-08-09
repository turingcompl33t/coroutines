# algorithms/when-all/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-when-all CXX)

include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(example "example.cpp")
target_link_libraries(example PRIVATE coro_config libcoro stdcoro)