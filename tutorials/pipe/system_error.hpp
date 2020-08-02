// system_error.hpp

#ifndef CORO_SYSTEM_ERROR_HPP
#define CORO_SYSTEM_ERROR_HPP

#include <errno.h>

namespace coro
{
    class system_error
    {
        int const code;

    public:
        system_error(int const code_ = errno)
            : code{code_} {}

        int get() const noexcept
        {
            return code;
        }
    };
}

#endif // CORO_SYSTEM_ERROR_HPP