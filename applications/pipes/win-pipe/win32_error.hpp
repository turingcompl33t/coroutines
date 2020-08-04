// win32_error.hpp

#ifndef CORO_WIN32_ERROR_HPP
#define CORO_WIN32_ERROR_HPP

#include <windows.h>

namespace coro
{
    class win32_error
    {
        unsigned long const code;
    public:
        win32_error(unsigned long const code_ = ::GetLastError())
            : code{code_}
        {}

        [[nodiscard]]
        unsigned long get() const noexcept
        {
            return code;
        }
    };
}

#endif // CORO_WIN32_ERROR_HPP