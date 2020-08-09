# applications/timers/nix-timerfd/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(nix-timerfd CXX)

include("../../../cmake/coro_config.cmake")

add_subdirectory("../../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(vanilla "vanilla.cpp")
target_link_libraries(vanilla coro_config libcoro stdcoro)

add_executable(awaitable "awaitable.cpp")
target_link_libraries(awaitable coro_config libcoro stdcoro)