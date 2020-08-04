// future.hpp

#ifndef CORO_FUTURE_HPP
#define CORO_FUTURE_HPP

#include <future>
#include <stdexcept>
#include <coroutine>

namespace coro
{   
    template <typename R>
    struct future_promise_type
    {
        std::promise<R> promise;

        std::future<R> get_return_object()
        {
            return promise.get_future();
        }

        auto initial_suspend()
        {
            return std::suspend_never{};
        }

        auto final_suspend()
        {
            return std::suspend_never{};
        }

        void return_void() 
        {
            promise.set_value();
        }

        template <typename T>
        void return_value(T&& value)
        {
            promise.set_value(std::forward<T>(value));
        }

        void unhandled_exception()
        {
            promise.set_exception(std::current_exception());
        }
    };
}

template <typename R, typename... Args>
struct std::coroutine_traits<std::future<R>, Args...>
{   
    using promise_type = coro::future_promise_type<R>;
};


#endif // CORO_FUTURE_HPP