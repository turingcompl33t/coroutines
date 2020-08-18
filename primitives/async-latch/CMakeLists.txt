# primitives/async-latch/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-keywords CXX)

include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(driver "driver.cpp")
target_link_libraries(driver PRIVATE coro_config libcoro stdcoro)