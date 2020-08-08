// nix_system_error.hpp
// Punting on system errors.

#ifndef NIX_SYSTEM_ERROR_HPP
#define NIX_SYSTEM_ERROR_HPP

#include <errno.h>

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

#endif // NIX_SYSTEM_ERROR_HPP