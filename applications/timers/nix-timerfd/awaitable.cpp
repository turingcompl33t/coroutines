// awaitable.cpp

#include "unique_fd.hpp"
#include "eager_resumable.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <coroutine>

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

constexpr static auto const DEFAULT_N_EXPIRATIONS = 5;

class system_error
{
    int const code;

public:
    explicit system_error(int const code_ = errno)
        : code{code_}
    {}

    int get() const noexcept
    {
        return code;
    }
};

class awaitable_timer
{
public:
    struct async_context
    {   
        std::coroutine_handle<> awaiting_coro;
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
            throw system_error{};
        }

        struct epoll_event ev{};
        ev.events   = EPOLLIN;
        ev.data.ptr = static_cast<void*>(&async_ctx);

        int const res = ::epoll_ctl(ioc, EPOLL_CTL_ADD, fd, &ev);
        if (-1 == res)
        {
            throw system_error{};
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
                auto const armed = me->rearm();
                return armed;
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

void error_exit(
    char const* msg, 
    int const code = errno)
{
    printf("[-] %s failed (%d)\n", msg, code);
    ::exit(EXIT_FAILURE);
}

void reactor(int const epoller, int const n_expirations)
{
    struct epoll_event ev{};

    for (int count = 0; count < n_expirations; ++count)
    {
        int const n_events = ::epoll_wait(epoller, &ev, 1, -1);
        if (-1 == n_events)
        {
            error_exit("epoll_wait()");
        }

        if (ev.events & EPOLLIN)
        {
            awaitable_timer::on_timer_expire(ev.data.ptr);
        }
    }
}

[[nodiscard]]
coro::eager_resumable waiter(
    int const ioc, 
    int const n_expirations)
{
    using namespace std::chrono_literals;

    awaitable_timer timer{ioc, 1s};

    for (auto count = 0; count < n_expirations; ++count)
    {
        co_await timer;

        puts("[+] timer fired");
    }

    co_return;
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    auto const n_expirations = argc > 1 
        ? std::atoi(argv[1]) 
        : DEFAULT_N_EXPIRATIONS;

    // create the epoll instance
    auto instance = unique_fd{::epoll_create1(0)};
    if (!instance)
    {
        error_exit("epoll_create1()");
    }

    auto waiter_res = waiter(instance.get(), n_expirations);

    reactor(instance.get(), n_expirations);

    return EXIT_SUCCESS;
}