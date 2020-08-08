# basics/composition/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-composition CXX)

include("../../cmake/coro_config.cmake")

add_executable(example0 "example0.cpp")
target_link_libraries(example0 PRIVATE coro_config)
target_include_directories(example0 PRIVATE "../../config")

add_executable(example1 "example1.cpp")
target_link_libraries(example1 PRIVATE coro_config)
target_include_directories(example1 PRIVATE "../../config")

add_executable(example2 "example2.cpp")
target_link_libraries(example2 PRIVATE coro_config)
target_include_directories(example2 PRIVATE "../../config")