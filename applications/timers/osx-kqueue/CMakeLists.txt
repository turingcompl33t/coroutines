# applications/timers/osx-kqueue/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(coro-timer-osx-kqueue CXX)

include("../../../cmake/coro_config.cmake")

add_executable(vanilla "vanilla.cpp")
target_link_libraries(vanilla PRIVATE coro_config)

add_executable(awaitable "awaitable.cpp")
target_link_libraries(awaitable PRIVATE coro_config)
target_include_directories(awaitable PRIVATE "../../../config/")