// eager_resumable.hpp

#ifndef CORO_EAGER_RESUMABLE_HPP
#define CORO_EAGER_RESUMABLE_HPP

#include <stdexcept>
#include <coroutine>

namespace coro
{   
    struct eager_resumable;

    struct eager_promise
    {
        eager_resumable get_return_object();

        auto initial_suspend()
        {
            return std::suspend_never{};
        }

        auto final_suspend()
        {
            return std::suspend_never{};
        }

        void return_void() {}

        void unhandled_exception()
        {
            std::terminate();
        }
    };

    class eager_resumable
    {
    public:
        using promise_type     = eager_promise;
        using coro_handle_type = std::coroutine_handle<eager_promise>;

        eager_resumable(coro_handle_type coro_handle_)
            : coro_handle{coro_handle_} {}

        ~eager_resumable()
        {
            if (coro_handle)
            {
                coro_handle.destroy();
            }
        }
    private:
        coro_handle_type coro_handle;
    };

    eager_resumable eager_promise::get_return_object()
    {
        return eager_resumable{std::coroutine_handle<eager_promise>::from_promise(*this)};
    }
}

#endif // CORO_EAGER_RESUMABLE_HPP