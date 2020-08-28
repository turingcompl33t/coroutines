# applications/map/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-map CXX)

include("../../cmake/threads.cmake")
include("../../cmake/warnings.cmake")
include("../../cmake/coro_config.cmake")

add_subdirectory("../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)
add_subdirectory("../../deps/catch2" ${CMAKE_CURRENT_BINARY_DIR}/catch2)

add_executable(test "test.cpp")
target_link_libraries(test PRIVATE coro_config libcoro stdcoro Catch2 warnings threads)

add_executable(driver "driver.cpp")
target_link_libraries(driver PRIVATE coro_config libcoro stdcoro Catch2 warnings threads)