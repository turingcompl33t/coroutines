# algorithms/when-all/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-when-all CXX)

include("../../cmake/coro_config.cmake")

add_executable(test "test.cpp")
target_link_libraries(test PRIVATE coro_config)
target_include_directories(test PRIVATE "../../config")