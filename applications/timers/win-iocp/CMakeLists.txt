# applications/timers/win-iocp/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(win-iocp CXX)

include("../../../cmake/warnings.cmake")
include("../../../cmake/coro_config.cmake")

add_subdirectory("../../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(driver_vanilla "driver_vanilla.cpp")
target_link_libraries(driver_vanilla PRIVATE coro_config libcoro stdcoro warnings)

add_executable(driver_awaitable "driver_awaitable.cpp")
target_link_libraries(driver_awaitable PRIVATE coro_config libcoro stdcoro warnings)