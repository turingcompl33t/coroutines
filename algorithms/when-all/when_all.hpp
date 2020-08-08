// when_all.hpp

#ifndef CORO_WHEN_ALL_HPP
#define CORO_WHEN_ALL_HPP

#include <vector>
#include <coroutine.hpp>

#include "task.hpp"

struct counter
{
    // The number of tasks remaining to complete.
    std::size_t count;

    // The top-level coroutine that co_awaits upon when_all().
    coro::coroutine_handle<> awaiting_coro;

    // here we initialize the count of number of remaining tasks
    // to the number of subtasks + 1 in order to avoid the race 
    // between completion of all of the subtasks and the call to
    // try_await() by the top-level when_all_awaiter
    counter(std::size_t count_)
        : count{count_ + 1} {}

    bool try_await(coro::coroutine_handle<> awaiting_coro_)
    {
        // invoked by the top-level when_all_awaiter directly after
        // it initiates each of the subtasks via call to start()

        // in the event that all of the subtasks passed to when_all()
        // complete prior to the invocation of try_await(), the count
        // remains > 1 because of the initialization of count to the
        // number of subtasks + 1, then when we reach this point, the
        // top-level when_all_awaiter invokes this function in its
        // await_suspend() member, determines that all tasks have completed,
        // and is able to synchronously return to the original caller
        // without ever suspending; otherwise, one or more tasks have
        // yet to complete, so we remove the added count value and suspend
        // and wait for completion of the final subtask to trigger
        // continuation of the original awaiter

        awaiting_coro = awaiting_coro_;
        return (--count > 1);
    }

    void notify_task_completion()
    {
        // invoked in the final_suspend() for coroutines 
        // created by make_when_all_task(); in the event
        // that the task that completes is the final task,
        // the top-level coroutine that originally co_awaited
        // upon when_all() is resumed 

        if (--count == 0)
        {
            awaiting_coro.resume();
        }
    }
};

struct when_all_task_promise;

struct when_all_task
{
    using promise_type     = when_all_task_promise;
    using coro_handle_type = coro::coroutine_handle<promise_type>;

    // the coroutine handle for a when_all_task refers to the
    // coroutine frame created for the make_when_all_task() coroutine
    coro_handle_type coro_handle;

    when_all_task(coro_handle_type coro_handle_)
        : coro_handle{coro_handle_} {}

    ~when_all_task()
    {
        if (coro_handle)
        {
            coro_handle.destroy();
        }
    }

    void start(counter& remaining_counter);
};

struct when_all_task_promise
{
    using coro_handle_type = when_all_task::coro_handle_type;

    counter& remaining_counter;

    when_all_task get_return_object()
    {
        return when_all_task{coro_handle_type::from_promise(*this)};
    }

    auto initial_suspend()
    {
        // preserve laziness of task produced by when_all()
        return coro::suspend_always{};
    }

    auto final_suspend()
    {
        // final_suspend() for the when_all_task_promise type 
        // is invoked when the associated task completes execution
        
        struct final_awaiter
        {
            when_all_task_promise& me;

            final_awaiter(when_all_task_promise& me_)
                : me{me_} {}

            bool await_ready() { return false; }

            void await_suspend(coro::coroutine_handle<when_all_task_promise>)
            {
                me.remaining_counter.notify_task_completion();
            }

            void await_resume() {}
        };

        return final_awaiter{*this};
    }

    void return_void() {}

    void unhandled_exception()
    {
        std::terminate();
    }

    void start(counter& remaining_counter_)
    {
        remaining_counter = remaining_counter_;
        coro_handle_type::from_promise(*this).resume();
    }
};

void when_all_task::start(counter& remaining_counter)
{
    coro_handle.promise().start(remaining_counter);
}

when_all_task make_when_all_task(task& t)
{
    // make_when_all_task() serves as a sort of "intermediary coroutine"
    // between the coroutine for one of the tasks upon which we want 
    // to wait and the top-level when_all_awaiter
    //
    // the first time that make_when_all_task() is invoked in the
    // body of when_all(), execution suspends prior to co_awaiting
    // on the task; this preserves the laziness of the task type
    // in that when_all() returns a new task that itself does not
    // begin execution of its "subtasks" until it is awaited

    co_await t;
}

struct when_all_awaiter
{
    // The collection of tasks to complete.
    std::vector<when_all_task>& tasks;
    
    // The number of tasks that remain to complete.
    counter remaining;

    // Handle to the awaiting coroutine.
    coro::coroutine_handle<> continuation;

    when_all_awaiter(std::vector<when_all_task>& tasks_)
        : tasks{tasks_}
        , remaining{tasks.size()} {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(coro::coroutine_handle<> awaiting_coro)
    {
        continuation = awaiting_coro;
        
        // initiate all of the subtasks
        for (auto& t : tasks)
        {
            t.start(remaining);
        }

        return remaining.try_await(awaiting_coro);
    }

    void await_resume() 
    {
        continuation.resume();
    }
};

when_all_awaiter when_all(std::vector<task>& input_tasks)
{
    std::vector<when_all_task> tasks{};
    tasks.reserve(input_tasks.size());

    for (auto& t : input_tasks)
    {
        tasks.emplace_back(make_when_all_task(t));
    }

    return when_all_awaiter{tasks};
}

#endif // CORO_WHEN_ALL_HPP