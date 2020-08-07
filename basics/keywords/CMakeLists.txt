# basics/keywords/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-keywords CXX)

include("../../cmake/set_coro_options.cmake")

message(${CORO_OPTIONS})

add_executable(await "co_await.cpp")
target_compile_options(await PRIVATE ${CORO_OPTIONS})