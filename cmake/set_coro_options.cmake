# set_options.cmake

# Configuring the system default for gcc/g++:
# https://askubuntu.com/questions/26498/how-to-choose-the-default-gcc-and-g-version

include(CheckCXXCompilerFlag)

if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    check_cxx_compiler_flag(-std=c++2a cxx_latest)
    check_cxx_compiler_flag(-fcoroutines-ts cxx_coroutines)
    set(CORO_OPTIONS -std=c++2a -fcoroutines-ts)
elseif(MSVC)   
    check_cxx_compiler_flag(/std:c++latest cxx_latest)
    check_cxx_compiler_flag(/await cxx_coroutines)
    set(CORO_OPTIONS /std:c++latest /await)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    check_cxx_compiler_flag(-std=gnu++2a cxx_latest)
    check_cxx_compiler_flag(-fcoroutines cxx_coroutines)
    set(CORO_OPTIONS -std=gnu++2a -fcoroutines)
endif()

if(NOT cxx_coroutines)
    message(FATAL_ERROR "builds in this repository require coroutines support")
endif()