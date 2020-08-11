# applications/timers/win-efficient/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(win-efficient CXX)

include("../../../cmake/coro_config.cmake")

add_subdirectory("../../../libcoro" ${CMAKE_CURRENT_BINARY_DIR}/libcoro)
add_subdirectory("../../../stdcoro" ${CMAKE_CURRENT_BINARY_DIR}/stdcoro)

add_executable(driver "driver.cpp" "timer_service.cpp")
target_link_libraries(driver coro_config libcoro stdcoro)
target_compile_definitions(driver PRIVATE NOMINMAX)