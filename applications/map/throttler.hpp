// throttler.hpp

#ifndef THROTTLER_HPP
#define THROTTLER_HPP

#include <iostream>

template <typename Scheduler>
class Throttler
{
    // the scheduler used by the throttler instance
    Scheduler const& scheduler;

    // the current number of remaining coroutines this instance may spawn
    std::size_t limit;

public:
    Throttler(
        Scheduler const&  scheduler_, 
        std::size_t const max_concurrent_tasks) 
        : scheduler{scheduler_}
        , limit{max_concurrent_tasks} {}

    ~Throttler()
    {
        run();
    }

    // non-copyable
    Throttler(Throttler const&)            = delete;
    Throttler& operator=(Throttler const&) = delete;

    // non-movable
    Throttler(Throttler&&)            = delete;
    Throttler& operator=(Throttler&&) = delete;

    template <typename TaskType>
    void spawn(TaskType task)
    {
        if (0 == limit)
        {
            auto handle = scheduler.remove_next_task();
            handle.resume();
        }

        // add the handle for the task to the scheduler queue
        auto handle = task.set_owner(this);
        scheduler.schedule(handle);
        
        // spawned a new active task, so decrement the limit
        --limit;
    }

    void run()
    {
        scheduler.run();
    }

    void on_task_complete()
    {
        ++limit;
    }
};

#endif // THROTTLER_HPP