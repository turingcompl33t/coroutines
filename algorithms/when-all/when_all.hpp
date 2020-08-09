// when_all.hpp

#ifndef WHEN_ALL_HPP
#define WHEN_ALL_HPP

#include <atomic>
#include <vector>
#include <utility>

#include <libcoro/task.hpp>
#include <stdcoro/coroutine.hpp>

struct counter
{
    // The number of tasks remaining to complete.
    std::atomic_size_t count;

    // The top-level coroutine that co_awaits upon when_all().
    stdcoro::coroutine_handle<> awaiting_coro;

    // here we initialize the count of number of remaining tasks
    // to the number of subtasks + 1 in order to avoid the race 
    // between completion of all of the subtasks and the call to
    // try_await() by the top-level when_all_awaiter
    counter(std::size_t count_)
        : count{count_ + 1} {}

    bool try_await(stdcoro::coroutine_handle<> awaiting_coro_)
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
        return (count.fetch_sub(1, std::memory_order_acq_rel) > 1);
    }

    void notify_task_completion()
    {
        // invoked in the final_suspend() for coroutines 
        // created by make_when_all_task(); in the event
        // that the task that completes is the final task,
        // the top-level coroutine that originally co_awaited
        // upon when_all() is resumed 

        if (count.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // notice that while it appears there is the potential got a 
            // race here between try_await() and notify_task_completion()
            // because notify_task_completion() might attempt to resume 
            // the awaiting coroutine before try_await() has the opporunity 
            // to set the handle to a valid coroutine handle, the reference 
            // counting semantics wherein we initialize the counter's count 
            // to n_awaited_tasks + 1 ensures that this cannot occur

            awaiting_coro.resume();
        }
    }
};

struct when_all_task_promise;

struct when_all_task
{
    using promise_type     = when_all_task_promise;
    using coro_handle_type = stdcoro::coroutine_handle<promise_type>;

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

    when_all_task(when_all_task const&) = delete;
    when_all_task& operator=(when_all_task const&) = delete;

    when_all_task(when_all_task&& t)
        : coro_handle{t.coro_handle}
    {
        t.coro_handle = nullptr;
    }

    when_all_task& operator=(when_all_task&& t)
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

    void start(counter& remaining_counter);
};

struct when_all_task_promise
{
    using coro_handle_type = when_all_task::coro_handle_type;

    counter* remaining_counter;

    when_all_task_promise() 
        : remaining_counter{nullptr} {}

    when_all_task get_return_object()
    {
        return when_all_task{coro_handle_type::from_promise(*this)};
    }

    auto initial_suspend()
    {
        // preserve laziness of task produced by when_all()
        return stdcoro::suspend_always{};
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

            void await_suspend(stdcoro::coroutine_handle<when_all_task_promise>)
            {
                me.remaining_counter->notify_task_completion();
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
        remaining_counter = &remaining_counter_;

        // this resumption does not yet resume the coroutine 
        // for the task originally passed to when_all() by
        // the user, but instead resumes the make_when_all_task()
        // coroutine which co_awaits on the user-provided task in
        // order to (finally) resume the coroutine we care about

        coro_handle_type::from_promise(*this).resume();
    }
};

void when_all_task::start(counter& remaining_counter)
{
    // we defer the start() operation to the start() member
    // of the promise type such that the promise type has the
    // opportunity to store a reference (actually a raw pointer)
    // to the counter so that it may notify the counter on completion

    coro_handle.promise().start(remaining_counter);
}

when_all_task make_when_all_task(coro::task<void> t)
{
    // make_when_all_task() serves as a sort of "intermediary coroutine"
    // between the coroutine for one of the tasks upon which we want 
    // to wait and the top-level when_all_awaiter

    // the first time that make_when_all_task() is invoked in the
    // body of when_all(), execution suspends prior to co_awaiting
    // on the task; this preserves the laziness of the task type
    // in that when_all() returns a new task that itself does not
    // begin execution of its "subtasks" until it is awaited

    co_await static_cast<coro::task<void>&&>(t);
}

// The top-level awaiter returned to the caller in which
// the co_await when_all() statement appears
struct when_all_awaiter
{
    // The collection of tasks to complete.
    std::vector<when_all_task> tasks;
    
    // The number of tasks that remain to complete.
    counter remaining;

    when_all_awaiter(std::vector<when_all_task>&& tasks_)
        : tasks{std::move(tasks_)}
        , remaining{tasks.size()} {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(stdcoro::coroutine_handle<> awaiting_coro)
    {
        // the user has now created a when_all_awaiter via a
        // call to when_all() and subsequently co_awaited upon
        // the returned task; it is at this point that we need 
        // to initiate each of the subtasks that are passed to 
        // the algorithm

        // note that, in contrast to "typical" awaiter patters, 
        // the when_all_awaiter does not maintain a handle for 
        // the awaiting coroutine; this is because the responsibility
        // of resuming the awaiting coroutine is deferred to the
        // final task in the set of tasks passed to when_all()
        
        // initiate all of the subtasks
        for (auto& t : tasks)
        {
            t.start(remaining);
        }

        // it is possible that at this point all of the subtasks
        // completed synchronously and thus that the task
        // created by when_all() is ready; the try_await() member
        // of the counter checks for this case, and will return 
        // `false` here such that the coroutine does not suspend

        return remaining.try_await(awaiting_coro);
    }

    void await_resume() {}
};

when_all_awaiter when_all(std::vector<coro::task<void>> input_tasks)
{
    // construct a new vector of when_all_task "subtasks";
    // each of the when_all_tasks takes ownership of the 
    // task for which it is responsible, so the input vector
    // of tasks provided by the user is cannibalized 

    std::vector<when_all_task> tasks{};
    tasks.reserve(input_tasks.size());

    for (auto& t : input_tasks)
    {
        tasks.emplace_back(make_when_all_task(std::move(t)));
    }

    return when_all_awaiter{std::move(tasks)};
}

#endif // WHEN_ALL_HPP