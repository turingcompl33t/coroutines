// task_scheduler.hpp
// 
// A weak "task scheduler" to support fine-grained
// controlled of coroutine resumption without having
// to wait on some external event source.

#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include <queue>
#include <coroutine.hpp>

class task_scheduler
{
    std::queue<coro::coroutine_handle<>> tasks;

public:
    task_scheduler() 
        : tasks{} {}

    ~task_scheduler() = default;

    void schedule(coro::coroutine_handle<> task)
    {
        tasks.push(task);
    }

    void run()
    {
        while (!tasks.empty())
        {
            tasks.front().resume();
            tasks.pop();
        }
    }
};

// suspend the awaiting coroutine and schedule
// it for resumption on the task scheduler
auto defer_on(task_scheduler& scheduler)
{
    struct awaitable
    {
        task_scheduler& scheduler;

        awaitable(task_scheduler& scheduler_)
            : scheduler{scheduler_} {}

        bool await_ready() { return false; }

        void await_suspend(coro::coroutine_handle<> awaiting_coro)
        {
            scheduler.schedule(awaiting_coro);
        }

        void await_resume() {}
    };

    return awaitable{scheduler};
}

#endif // TASK_SCHEDULER_HPP