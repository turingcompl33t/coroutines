// win/system_error.hpp

#ifndef CORO_WIN_SYSTEM_ERROR_HPP
#define CORO_WIN_SYSTEM_ERROR_HPP

#include <windows.h>

namespace coro::win
{
    class system_error
    {
        unsigned long const code;
    public:
        explicit system_error(
            unsigned long const code_ = ::GetLastError())
            : code{code_}
        {}

        unsigned long get() const noexcept
        {
            return code;
        }
    };
}

#endif // CORO_WIN_SYSTEM_ERROR_HPP