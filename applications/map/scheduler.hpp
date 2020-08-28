// scheduler.hpp

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <cstdlib>
#include <stdcoro/coroutine.hpp>

class Scheduler
{
    constexpr static auto const MAX_TASKS{256};
    
    using CoroHandle = std::coroutine_handle<>;

    std::size_t head{0};
    std::size_t tail{0};

    CoroHandle buffer[MAX_TASKS];

public:
    void push_back(CoroHandle handle)
    {
        buffer[head] = handle;
        head = (head + 1) % MAX_TASKS;
    }

    CoroHandle pop_front()
    {
        auto result = buffer[tail];
        tail = (tail + 1) % MAX_TASKS;
        return result;
    }

    auto try_pop_front()
    {
        return head != tail ? pop_front() : CoroHandle{};
    }

    void run()
    {
        while (auto handle = try_pop_front())
        {
            handle.resume();
        }
    }
};

inline Scheduler inline_scheduler;

#endif // SCHEDULER_HPP