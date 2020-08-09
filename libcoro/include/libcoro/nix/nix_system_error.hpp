// nix_system_error.hpp

#ifndef CORO_NIX_SYSTEM_ERROR_HPP
#define CORO_NIX_SYSTEM_ERROR_HPP

#include <errno.h>

namespace coro
{
    // nix_system_error
    //
    // A simple way to capture and propagate system-level errors.

    class nix_system_error
    {
        int const code;

    public:
        nix_system_error(int const code_ = errno)
            : code{code_} {}
        
        int get() const noexcept
        {
            return code;
        }
    };
}

#endif // CORO_NIX_SYSTEM_ERROR_HPP