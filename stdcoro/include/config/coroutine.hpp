// coroutine.hpp
//
// Internal configuration to account for different 
// stages of compiler support for coroutines TS.
//
// Adapted from config.hpp from cppcoro library:
// https://github.com/lewissbaker/cppcoro/blob/master/include/cppcoro/config.hpp

// detect compiler
// NOTE: the order of these checks is important because clang on OSX
// defines both __clang__ as well as __GNUC__ so we need to either 
// disambiguate via a nested condition or assume that if __clang__ is
// defined then clang is the compiler that we will use for the platform

#ifndef CONFIG_COROUTINE_CONFIG_HPP
#define CONFIG_COROUTINE_CONFIG_HPP

#if defined(__clang__)
    #define CORO_COMPILER_CLANG (__clang_major__ * 10000 + \
                                 __clang_minor__ * 100 + \
                                 __clang_patchlevel__)
    #define CORO_COMPILER_GCC   0
    #define CORO_COMPILER_MSVC  0
#elif defined(__GNUC__) || defined(__GNUG__)
    #define CORO_COMPILER_CLANG 0
    #define CORO_COMPILER_GCC (__GNUC__ * 10000 + \
                               __GNUC_MINOR__ * 100 + \
                               __GNUC_PATCHLEVEL__)
    #define CORO_COMPILER_MSVC  0
#elif defined(_MSC_VER)
    #define CORO_COMPILER_CLANG 0
    #define CORO_COMPILER_GCC   0
    #define CORO_COMPILER_MSVC  _MSC_FULL_VER
#else
    #error "unrecognized compiler"
#endif 

// include the appropriate coroutine header based on compiler
#if CORO_COMPILER_MSVC
    #include <experimental/coroutine>
#elif CORO_COMPILER_CLANG
    #include <experimental/coroutine>
#else
    #include <coroutine>
#endif 

//  namespace alias
#if CORO_COMPILER_GCC
    namespace stdcoro = std;
#else
    namespace stdcoro = std::experimental;
#endif

#endif // CONFIG_COROUTINE_CONFIG_HPP