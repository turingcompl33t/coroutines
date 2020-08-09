// task.hpp

#ifndef CORO_TASK_HPP
#define CORO_TASK_HPP

#include <utility>
#include <stdexcept>
#include <coroutine.hpp>

struct task_promise;

struct task_awaiter;

class task
{
    coro::coroutine_handle<task_promise> coro_handle;

public:
    using promise_type     = task_promise;
    using coro_handle_type = coro::coroutine_handle<task_promise>;

    task(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    ~task()
    {
        if (coro_handle)
        {
            coro_handle.destroy();
        }
    }

    task(task const&)            = delete;
    task& operator=(task const&) = delete;

    task(task&& t) 
        : coro_handle{t.coro_handle} 
    {
        t.coro_handle = nullptr;
    }

    task& operator=(task&& t)
    {
        if (std::addressof(t) != this)
        {
            if (coro_handle)
            {
                coro_handle.destroy();
            }

            coro_handle   = t.coro_handle;
            t.coro_handle = nullptr;
        }

        return *this;
    }

    task_awaiter operator co_await();

    bool resume() 
    {
        if (!coro_handle.done())
        {
            coro_handle.resume();
        }

        return !coro_handle.done();
    }
};

struct task_promise
{
    coro::coroutine_handle<> continuation;

    task get_return_object()
    {
        return task{coro::coroutine_handle<task_promise>::from_promise(*this)};
    }

    auto initial_suspend()
    {
        return coro::suspend_always{};
    }

    auto final_suspend()
    {
        struct final_awaiter
        {
            task_promise& me;

            bool await_ready() { return false; }

            void await_suspend(coro::coroutine_handle<> handle_)
            {
                if (me.continuation)
                {
                    me.continuation.resume();
                }
            }

            void await_resume() {}
        };

        return final_awaiter{*this};
    }

    void return_void() {}

    void unhandled_exception() noexcept
    {
        std::terminate();
    }
};

struct task_awaiter
{
    using coro_handle_type = task::coro_handle_type;

    coro_handle_type coro_handle;

    task_awaiter(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    bool await_ready()
    {
        return false;
    }

    void await_suspend(coro::coroutine_handle<> awaiting_coro)
    {
        coro_handle.promise().continuation = awaiting_coro;
        coro_handle.resume();
    }

    void await_resume() {}
};

task_awaiter task::operator co_await()
{
    return task_awaiter{coro_handle};
}

#endif // CORO_TASK_HPP