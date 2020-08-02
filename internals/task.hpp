// task.hpp

#ifndef TASK_H
#define TASK_H

#include <cassert>
#include <stdexcept>
#include <coroutine>

struct task
{
    struct promise_type;
    using coro_handle = std::coroutine_handle<promise_type>;

    struct promise_type
    {
        task get_return_object()
        {
            return coro_handle::from_promise(*this);
        }

        auto initial_suspend() 
        { 
            return std::suspend_never{}; 
        }
        
        auto final_suspend() 
        {
            return std::suspend_always{}; 
        }

        void return_void()
        {}

        auto unhandled_exception()
        {
            std::terminate();
        }
    };

    task(coro_handle handle_) 
        : handle{handle_} 
    {
        assert(handle);
    }

    ~task()
    {
        handle.destroy();
    }

    bool resume()
    {
        if (!handle.done())
        {
            handle.resume();
        }

        return !handle.done();
    }

private:
    coro_handle handle;
};

#endif // TASK_H