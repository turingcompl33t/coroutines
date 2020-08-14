// awaitable_timer.hpp

#ifndef AWAITABLE_TIMER_HPP
#define AWAITABLE_TIMER_HPP

#include <utility>
#include <stdcoro/coroutine.hpp>

#include <windows.h>

#include <libcoro/win/system_error.hpp>
#include <libcoro/win/unique_handle.hpp>

#include "io_context.hpp"

// ----------------------------------------------------------------------------
// Utility

template <typename Duration>
static void timeout_to_filetime(Duration duration, FILETIME* ft)
{   
    ULARGE_INTEGER tmp{};

    auto const ns = std::chrono::duration_cast<
        std::chrono::nanoseconds>(duration);

    // filetime represented in 100 ns increments
    auto const ticks = (ns.count() / 100);

    // lazy way to split the high and low words 
    tmp.QuadPart = -(ticks);

    ft->dwLowDateTime  = tmp.LowPart;
    ft->dwHighDateTime = tmp.HighPart;
}

// ----------------------------------------------------------------------------
// awaitable_timer

class timer_awaiter;

class awaitable_timer
{
public:
    struct async_context
    {
        std::experimental::coroutine_handle<> awaiting_coro;
    };

private:
    // The associated IO context.
    io_context& ioc;

    // The timout for this timer as a native filetime.
    FILETIME timeout;

    // The owned timer object.
    coro::win::tp_timer_handle timer;

    // The context utilized in timer expiration callback.
    async_context async_ctx;

public:
    template <typename Duration>
    explicit awaitable_timer(io_context& ioc_, Duration timeout_)
        : ioc{ioc_}, timeout{}, timer{}, async_ctx{}
    {
        // convert the specified timeout
        timeout_to_filetime(timeout_, &timeout);

        // create the underlying timer object
        auto* timer_obj = ::CreateThreadpoolTimer(
            on_timer_expiration, 
            &async_ctx, 
            ioc.env());

        if (NULL == timer_obj)
        {
            throw coro::win::system_error{};
        }

        timer.reset(timer_obj);
    }

    ~awaitable_timer() = default;

    // non-copyable
    awaitable_timer(awaitable_timer const&)            = delete;
    awaitable_timer& operator=(awaitable_timer const&) = delete;

    awaitable_timer(awaitable_timer&& at) 
        : ioc{at.ioc}
        , timeout{at.timeout}
        , timer{std::move(at.timer)}
        , async_ctx{at.async_ctx}
    {}

    awaitable_timer& operator=(awaitable_timer&& at)
    {
        if (std::addressof(at) != this)
        {
            close();

            ioc       = at.ioc;
            timeout   = at.timeout;
            timer     = std::move(at.timer);
            async_ctx = at.async_ctx;
        }

        return *this;
    }

    timer_awaiter operator co_await();

    // Invoked by IO context thread on timer expiration.
    static void __stdcall on_timer_expiration(
        PTP_CALLBACK_INSTANCE, 
        void* ctx, 
        PTP_TIMER)
    {
        // resume the awaiting coroutine
        auto& async_ctx = *reinterpret_cast<async_context*>(ctx);
        async_ctx.awaiting_coro.resume();
    }

    friend class timer_awaiter;

private:
    void close()
    {
        if (timer)
        {
            ::CloseThreadpoolTimer(timer.release());
        }
    }
};

// ----------------------------------------------------------------------------
// timer_awaiter

class timer_awaiter
{
    awaitable_timer& timer;
public:
    timer_awaiter(awaitable_timer& timer_)
        : timer{timer_}
    {}

    bool await_ready()
    {
        return false;
    }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coro)
    {
        timer.async_ctx.awaiting_coro = awaiting_coro;
        ::SetThreadpoolTimer(timer.timer.get(), &timer.timeout, 0, 0);
    }

    void await_resume() {}
};

timer_awaiter awaitable_timer::operator co_await()
{
    return timer_awaiter{*this};
}

#endif // AWAITABLE_TIMER_HPP