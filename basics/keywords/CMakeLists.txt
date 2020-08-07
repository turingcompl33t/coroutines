# basics/keywords/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-keywords CXX)

include("../../cmake/coro_config.cmake")

add_executable(await "await.cpp")
target_link_libraries(await PRIVATE coro_config)
target_include_directories(await PRIVATE "../../config")

add_executable(return "return.cpp")
target_link_libraries(return PRIVATE coro_config)
target_include_directories(return PRIVATE "../../config")

add_executable(yield "yield.cpp")
target_link_libraries(yield PRIVATE coro_config)
target_include_directories(yield PRIVATE "../../config")