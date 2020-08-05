// task_v1.hpp
//
// A more fully-featured asynchronous task type:
// - Added continuation support

#include <coroutine>
#include <stdexcept>

struct task_promise;
struct task_awaiter;

struct task
{
    using promise_type     = task_promise;
    using coro_handle_type = std::coroutine_handle<task_promise>;

    task(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    ~task()
    {
        if (coro_handle)
        {
            coro_handle.destroy();
        }
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

private:
    coro_handle_type coro_handle;
};

struct task_promise
{
    using coro_handle_type = task::coro_handle_type;

    coro_handle_type continuation;

    task_promise() : continuation{} {}

    task get_return_object()
    {
        return task{coro_handle_type::from_promise(*this)};
    }

    auto initial_suspend()
    {
        return std::suspend_always{};
    }

    auto final_suspend()
    {
        struct final_awaiter
        {
            task_promise& me;

            bool await_ready()
            {
                return false;
            }

            void await_suspend(coro_handle_type)
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

    task_awaiter(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    bool await_ready()
    {
        return false;
    }

    void await_suspend(coro_handle_type awaiting_coro_)
    {   
        coro_handle.promise().continuation = awaiting_coro_;
        coro_handle.resume();
    }

    void await_resume() {}

private:
    coro_handle_type coro_handle;
};

task_awaiter task::operator co_await()
{
    return task_awaiter{this->coro_handle};
}