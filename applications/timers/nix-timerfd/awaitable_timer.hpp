// awaitable_timer.hpp
//
// An awaitable system timer for Linux using the timerfd API.

#ifndef AWAITABLE_TIMER_HPP
#define AWAITABLE_TIMER_HPP

#include <chrono>
#include <utility>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <stdcoro/coroutine.hpp>

class awaitable_timer
{
public:
    struct async_context
    {   
        stdcoro::coroutine_handle<> awaiting_coro;
    };

private:
    // The IO context.
    int ioc;

    // The underlying file descriptor for the timer.
    int fd;

    // The second and nanosecond portions of timeout, respectively.
    std::chrono::seconds     sec;
    std::chrono::nanoseconds ns;
    
    // A handle to the awaiting coroutine;
    // set when awaited upon in await_suspend()
    async_context async_ctx;

public:
    template <typename Duration>
    awaitable_timer(int const ioc_, Duration timeout)
        : ioc{ioc_}
        , fd{::timerfd_create(CLOCK_REALTIME, 0)}
        , async_ctx{}
    {
        using namespace std::chrono;

        if (-1 == fd)
        {
            throw coro::nix_system_error{};
        }

        struct epoll_event ev{};
        ev.events   = EPOLLIN;
        ev.data.ptr = static_cast<void*>(&async_ctx);

        int const res = ::epoll_ctl(ioc, EPOLL_CTL_ADD, fd, &ev);
        if (-1 == res)
        {
            throw coro::nix_system_error{};
        }

        auto const sec_ = duration_cast<seconds>(timeout);
        auto const ns_  = duration_cast<nanoseconds>(timeout) 
            - duration_cast<nanoseconds>(sec_);

        sec = sec_;
        ns  = ns_;
    }

    ~awaitable_timer()
    {
        close();
    }

    awaitable_timer(awaitable_timer const&) = delete;
    awaitable_timer& operator=(awaitable_timer const&) = delete;

    awaitable_timer(awaitable_timer&& at)
        : fd{at.fd}, sec{at.sec}, ns{at.ns}
    {
        using namespace std::chrono_literals;

        at.fd  = -1;
        at.sec = 0s;
        at.ns  = 0ns;
    }

    awaitable_timer& operator=(awaitable_timer&& at)
    {
        using namespace std::chrono_literals;

        if (std::addressof(at) != this)
        {
            close();

            fd  = at.fd;
            sec = at.sec;
            ns  = at.ns;

            at.fd  = -1;
            at.sec = 0s;
            at.ns  = 0ns;
        }

        return *this;
    }

    auto operator co_await()
    {
        struct awaiter
        {
            awaitable_timer* me;

            awaiter(awaitable_timer* me_) : me{me_} {}

            bool await_ready()
            {
                return false;
            }

            bool await_suspend(std::coroutine_handle<> awaiting_coro)
            {
                me->async_ctx.awaiting_coro = awaiting_coro;
                return me->rearm();
            }

            void await_resume() {}
        };

        return awaiter{this};
    }

    static void on_timer_expire(void* ctx)
    {
        auto* async_ctx = static_cast<async_context*>(ctx);
        async_ctx->awaiting_coro.resume();
    }

private:
    bool rearm() noexcept
    {
        struct itimerspec spec = {
            .it_interval = { 0, 0 },
            .it_value = { sec.count(), ns.count() }
        };

        int const res = ::timerfd_settime(fd, 0, &spec, nullptr);
        return res != -1;
    }

    void close() noexcept
    {
        if (fd != -1)
        {
            ::epoll_ctl(ioc, EPOLL_CTL_DEL, fd, nullptr);
            ::close(fd);
        }
    }
};

#endif // AWAITABLE_TIMER_HPP