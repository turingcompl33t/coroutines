// task.hpp
// A simple task type for coroutines that do not return values.

#ifndef TASK_H
#define TASK_H

#include <coroutine>

struct task
{
    struct promise_type
    {
        task get_return_object() { return {}; }
        
        auto initial_suspend()
        {
            return std::suspend_never{};
        }

        auto final_suspend()
        {
            return std::suspend_never{};
        }

        void return_void() {}

        void unhandled_exception() {}
    };
};

#endif // TASK_H