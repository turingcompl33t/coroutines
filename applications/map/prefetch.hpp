// prefetch.hpp
// A type that supports the coroutine awaiter interface
// in order to enable awaitable hardware prefetching.

#ifndef PREFETCH_HPP
#define PREFETCH_HPP

#include <stdcoro/coroutine.hpp>

#include <xmmintrin.h>

template <typename T, typename Scheduler>
struct prefetch_awaitable
{
    T*               address;
    Scheduler const& scheduler;

    prefetch_awaitable(T* address_, Scheduler const& scheduler_) 
        : address{address_}, scheduler{scheduler_} {}

    bool await_ready()
    {
        // no way to query the system to determine if the address is
        // already present in cache, so just (pessimistically) assume
        // that it is not and thus that we have to suspend + prefetch

        return false;
    }

    auto await_suspend(stdcoro::coroutine_handle<> awaiting_coroutine)
    {
        // prefetch the desired value
        _mm_prefetch(reinterpret_cast<char const*>(address), _MM_HINT_NTA);

        // schedule the coroutine for resumption
        scheduler.schedule(awaiting_coroutine);

        // TODO: what if we don't have symmetric transfer?
        return scheduler.remove_next_task();
    }

    T* await_resume()
    {
        // assume the value is ready now
        return address;
    }
};

template <typename T, typename Scheduler>
auto prefetch_and_schedule_on(
    T*               address, 
    Scheduler const& scheduler)
{
    return prefetch_awaitable<T, Scheduler>{address, scheduler};
}

#endif // PREFETCH_HPP