// eager_resumable.hpp

#ifndef CORO_EAGER_RESUMABLE_HPP
#define CORO_EAGER_RESUMABLE_HPP

#include <stdexcept>
#include <experimental/coroutine>

class eager_resumable;

struct eager_promise
{    
    eager_resumable get_return_object();

    auto initial_suspend()
    {
        return std::experimental::suspend_never{};
    }

    auto final_suspend()
    {
        return std::experimental::suspend_never{};
    }

    void return_void() {}

    void unhandled_exception() noexcept
    {
        std::terminate();
    }
};

class eager_resumable
{
public:
    using promise_type = eager_promise;
    using coro_handle  = std::experimental::coroutine_handle<eager_promise>;

private:
    coro_handle handle;

public:
    eager_resumable(coro_handle handle_) 
        : handle{handle_} {}

    ~eager_resumable() = default;
};

eager_resumable eager_promise::get_return_object()
{
    return eager_resumable{
        std::experimental::coroutine_handle<eager_promise>::from_promise(*this)};
}

#endif // CORO_EAGER_RESUMABLE_HPP