// eager_task.hpp

#ifndef EAGER_TASK_HPP
#define EAGER_TASK_HPP

#include <stdexcept>
#include <coroutine.hpp>

struct eager_task_promise;

class eager_task
{
    coro::coroutine_handle<eager_task_promise> coro_handle;

public:
    using promise_type = eager_task_promise;
    using coro_handle_type = coro::coroutine_handle<promise_type>;

    eager_task(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    ~eager_task()
    {
        if (coro_handle)
        {
            coro_handle.destroy();
        }
    }
};

struct eager_task_promise
{
    using coro_handle_type = eager_task::coro_handle_type;

    eager_task get_return_object()
    {
        return eager_task{coro_handle_type::from_promise(*this)};
    }

    auto initial_suspend()
    {
        // eager
        return coro::suspend_never{};
    }

    auto final_suspend()
    {
        return coro::suspend_always{};
    }

    void return_void() {}

    void unhandled_exception() noexcept
    {
        std::terminate();
    }
};

#endif // EAGER_TASK_HPP

