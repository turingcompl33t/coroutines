// awaitable_timer.hpp
// An awaitable system timer compatible with OSX kqueue.

#ifndef AWAITABLE_TIMER_HPP
#define AWAITABLE_TIMER_HPP

#include <chrono>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <stdcoro/coroutine.hpp>

// enable use of designated initializers
#pragma clang diagnostic ignored "-Wc99-extensions"

class awaitable_timer
{
    struct async_context
    {
        stdcoro::coroutine_handle<> awaiting_coro;
    };

    // The IO context with which this timer is associated.
    int ioc;

    // The unique identifier for the timer.
    uintptr_t ident;

    // The timeout for the owned timer. 
    std::chrono::microseconds timeout;

    // A handle to the coroutine waiting for this timer to fire.
    async_context async_ctx;

public:
    template <typename Duration>
    awaitable_timer(int ioc_, uintptr_t ident_, Duration timeout_)
        : ioc{ioc_}
        , ident{ident_}
        , timeout{std::chrono::duration_cast<
            std::chrono::microseconds>(timeout_)}
        , async_ctx{}
    {}

    awaitable_timer(awaitable_timer const&)            = delete;
    awaitable_timer& operator=(awaitable_timer const&) = delete;

    awaitable_timer(awaitable_timer&&)            = delete;
    awaitable_timer& operator=(awaitable_timer&&) = delete;

    auto operator co_await()
    {
        struct awaiter
        {
            awaitable_timer& me;

            awaiter(awaitable_timer& me_)
                : me{me_} {}

            bool await_ready() { return false; }

            bool await_suspend(stdcoro::coroutine_handle<> awaiting_coro)
            {
                me.async_ctx.awaiting_coro = awaiting_coro;
                return me.arm_timer();
            }

            void await_resume() {}
        };

        return awaiter{*this};
    }

    static void on_timer_expiration(void* ctx)
    {
        auto& async_ctx = *reinterpret_cast<async_context*>(ctx);
        async_ctx.awaiting_coro.resume();
    }

private:
    bool arm_timer()
    {
        struct kevent ev = {
            .ident  = ident,
            .filter = EVFILT_TIMER,
            .flags  = EV_ADD | EV_ONESHOT,
            .fflags = NOTE_USECONDS,
            .data   = timeout.count(),
            .udata  = &async_ctx
        };

        int const result = ::kevent(ioc, &ev, 1, nullptr, 0, nullptr);
        return (result != -1);
    }
};

#endif // AWAITABLE_TIMER_HPP