// win32_error.hpp

#ifndef CORO_WIN32_ERROR_HPP
#define CORO_WIN32_ERROR_HPP

#include <windows.h>

struct win32_error
{
    unsigned long const code;
    win32_error(unsigned long const code_ 
        = ::GetLastError()) : code{code_}
    {}
};

#endif // CORO_WIN32_ERROR_HPP