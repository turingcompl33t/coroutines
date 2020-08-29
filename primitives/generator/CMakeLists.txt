# primitives/generator/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(generator CXX)

include("../../cmake/warnings.cmake")
include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)
add_subdirectory("../../deps/catch2" ${CMAKE_CURRENT_BINARY_DIR}/catch2)

add_executable(test "test.cpp")
target_link_libraries(test PRIVATE Catch2 coro_config libcoro stdcoro warnings)

add_executable(driver "driver.cpp")
target_link_libraries(driver PRIVATE coro_config libcoro stdcoro warnings)