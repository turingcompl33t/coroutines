# config/CmakeLists.txt

cmake_minimum_required(VERSION 3.17)

project(stdcoro CXX)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
    ${PROJECT_NAME} 
    INTERFACE 
    $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>)