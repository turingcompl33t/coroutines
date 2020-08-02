// task_queue.hpp

#ifndef CORO_TASK_QUEUE_HPP
#define CORO_TASK_QUEUE_HPP

#include <queue>
#include <coroutine>

#include "task.hpp"

namespace coro
{
    class task_queue
    {
        std::queue<std::coroutine_handle<>> tasks;

    public:
        task_queue() 
            : tasks{}
        {}

        ~task_queue()
        {
            run_all();
        }

        void schedule(std::coroutine_handle<> handle)
        {
            tasks.push(handle);
        }

        void run_all()
        {
            while (!tasks.empty())
            {
                auto task = tasks.front();
                tasks.pop();
                task.resume();
            }
        }
    };
}

#endif // CORO_TASK_QUEUE_HPP