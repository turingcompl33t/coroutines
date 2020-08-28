// static_queue_scheduler.hpp

#ifndef STATIC_QUEUE_SCHEDULER_HPP
#define STATIC_QUEUE_SCHEDULER_HPP

#include <cstdlib>
#include <stdcoro/coroutine.hpp>

class StaticQueueScheduler
{
    constexpr static auto const MAX_TASKS{64};
    
    using CoroHandle = std::coroutine_handle<>;

    mutable std::size_t head;
    mutable std::size_t tail;

    mutable CoroHandle buffer[MAX_TASKS];

public:
    StaticQueueScheduler()
        : head{0}, tail{0} {}

    // schedule a coroutine for later resumption
    void schedule(CoroHandle handle) const
    {
        buffer[head] = handle;
        head = (head + 1) % MAX_TASKS;
    }

    CoroHandle& get_next_task() const
    {
        return buffer[tail];
    }

    CoroHandle remove_next_task() const
    {
        auto result = buffer[tail];
        tail = (tail + 1) % MAX_TASKS;
        return result;
    }

    void run() const
    {
        while (auto handle = try_remove_next_task())
        {
            handle.resume();
        }
    }

private:
    auto try_remove_next_task()
    {
        return head != tail ? remove_next_task() : CoroHandle{};
    }
};

#endif // STATIC_QUEUE_SCHEDULER_HPP