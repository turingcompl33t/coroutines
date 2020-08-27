// prefetch.hpp
// A type that supports the coroutine awaiter interface
// in order to enable awaitable hardware prefetching.

#ifndef PREFETCH_HPP
#define PREFETCH_HPP

#include <stdcoro/coroutine.hpp>

#include <xmmintrin.h>

template <typename T>
struct prefetch_awaitable
{
    T& value;

    prefetch_awaitable(T& value_) 
        : value{value_} {}


    auto await_ready() -> bool
    {
        // no way to query the system to determine if the address is
        // already present in cache, so just (pessimistically) assume
        // that it is not and thus that we have to suspend + prefetch

        return false;
    }

    auto await_suspend(stdcoro::coroutine_handle<>) -> void
    {
        // prefetch the desired value
        _mm_prefetch(reinterpret_cast<char const*>(
            std::addressof(value)), _MM_HINT_NTA);
    }

    auto await_resume() -> T&
    {
        // assume the value is ready now
        return value;
    }
};

template <typename T>
auto prefetch(T& value)
{
    return prefetch_awaitable<T>{value};
}

#endif // PREFETCH_HPP