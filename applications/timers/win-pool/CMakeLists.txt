# applications/timers/win-pool/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(win-pool CXX)

include("../../../cmake/warnings.cmake")
include("../../../cmake/coro_config.cmake")

add_subdirectory("../../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(vanilla "vanilla.cpp")
target_link_libraries(vanilla PRIVATE coro_config libcoro warnings)

add_executable(awaitable "awaitable.cpp")
target_link_libraries(awaitable PRIVATE coro_config libcoro stdcoro warnings)