// runtime_error.hpp

#ifndef CORO_RUNTIME_ERROR_HPP
#define CORO_RUNTIME_ERROR_HPP

namespace coro
{
    class runtime_error
    {
        char const* msg;
    public:
        runtime_error(char const* msg_ = "")
            : msg{msg_}
        {}

        [[nodiscard]]
        char const* get() const noexcept
        {
            return msg;
        }
    };
}

#endif // CORO_RUNTIME_ERROR_HPP