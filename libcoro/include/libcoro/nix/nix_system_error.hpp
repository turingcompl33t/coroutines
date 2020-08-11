// nix/system_error.hpp

#ifndef CORO_NIX_SYSTEM_ERROR_HPP
#define CORO_NIX_SYSTEM_ERROR_HPP

#include <errno.h>

namespace coro::nix
{
    class system_error
    {
        int const code;

    public:
        explicit system_error(int const code_ = errno)
            : code{code_} {}
        
        int get() const noexcept
        {
            return code;
        }
    };
}

#endif // CORO_NIX_SYSTEM_ERROR_HPP