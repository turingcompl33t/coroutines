# algorithms/when-all/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-when-all CXX)

include("../../cmake/coro_config.cmake")

add_executable(example0 "example0.cpp")
target_link_libraries(example0 PRIVATE coro_config)
target_include_directories(example0 PRIVATE "../../config")
target_compile_options(example0 PRIVATE "-ggdb")