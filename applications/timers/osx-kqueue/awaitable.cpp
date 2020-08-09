// awaitable.cpp

#include <chrono>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <stdcoro/coroutine.hpp>
#include <libcoro/eager_task.hpp>
#include <libcoro/nix/unique_fd.hpp>
#include <libcoro/nix/nix_system_error.hpp>

// enable use of designated initializers
#pragma clang diagnostic ignored "-Wc99-extensions"

constexpr static auto const DEFAULT_N_REPS = 5ul;

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

coro::eager_task<void> waiter(int ioc, unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    // NOTE: in a library implementation, a nicer API would
    // have the timer derive its unique identifier from the IOC;
    // can't do that here because the IOC is just an FD 

    awaitable_timer timer{ioc, 0, 3s};

    for (auto i = 0ul; i < n_reps; ++i)
    {
        co_await timer;

        puts("[+] timer fired");
    }
}

void reactor(int ioc, unsigned long const n_reps)
{
    struct kevent ev{};

    for (auto i = 0ul; i < n_reps; ++i)
    {
        int const n_events = ::kevent(ioc, nullptr, 0, &ev, 1, nullptr);
        if (-1 == n_events)
        {
            puts("[-] kevent() error");
            break;
        }

        awaitable_timer::on_timer_expiration(ev.udata);
    }
}

int main(int argc, char* argv[])
{
    auto const n_reps = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    // create the kqueue instance
    auto ioc = coro::unique_fd{::kqueue()};
    if (!ioc)
    {
        throw coro::nix_system_error{};
    }

    // start the timer
    auto t = waiter(ioc.get(), n_reps);

    // run the reactor
    reactor(ioc.get(), n_reps);

    return EXIT_SUCCESS;
}