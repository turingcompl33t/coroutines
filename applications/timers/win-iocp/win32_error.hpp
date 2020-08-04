// win32_error.hpp

#ifndef WIN32_ERROR_HPP
#define WIN32_ERROR_HPP

#include <windows.h>

struct win32_error
{
    unsigned long const code;
    win32_error(unsigned long const code_ 
        = ::GetLastError())
        : code{code_} {}
};

#endif // WIN32_ERROR_HPP