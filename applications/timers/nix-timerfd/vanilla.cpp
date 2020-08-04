// vanilla.cpp

#include "unique_fd.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

constexpr static auto const DEFAULT_N_EXPIRATIONS = 5;

using expiration_callback_f = void (*)(int);

void on_timer_expiration(int timer_fd);

struct expiration_ctx
{
    int                   fd;
    expiration_callback_f cb;
};

void error_exit(
    char const* msg, 
    int const code = errno)
{
    printf("[-] %s failed (%d)\n", msg, code);
}

template <typename Duration>
void arm_timer(int timer_fd, Duration timeout)
{
    auto const secs 
        = std::chrono::duration_cast<std::chrono::seconds>(timeout);

    struct itimerspec spec = {
        .it_interval = { 0, 0 },
        .it_value = { secs.count(), 0 }
    };

    int const res = timerfd_settime(timer_fd, 0, &spec, nullptr);
    if (-1 == res)
    {
        error_exit("timerfd_settime()");
    }
}

void on_timer_expiration(int timer_fd)
{
    using namespace std::chrono_literals;

    puts("[+] timer fired");
    arm_timer(timer_fd, 2s);
}

void reactor(int const epoller, int const n_expirations)
{
    struct epoll_event ev{};

    for (int count = 0; count < n_expirations; ++count)
    {
        int const n_events = epoll_wait(epoller, &ev, 1, -1);
        if (-1 == n_events)
        {
            error_exit("epoll_wait()");
        }

        if (ev.events & EPOLLIN)
        {
            auto& ctx = *reinterpret_cast<expiration_ctx*>(ev.data.ptr);
            ctx.cb(ctx.fd);
        }
    }
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    auto const n_expirations = argc > 1 
        ? atoi(argv[1]) 
        : DEFAULT_N_EXPIRATIONS;

    // create the epoll instance
    auto instance = unique_fd{::epoll_create1(0)};
    if (!instance)
    {
        error_exit("epoll_create1()");
    }

    // create the timer
    auto timer = unique_fd{::timerfd_create(CLOCK_REALTIME, 0)};
    if (!timer)
    {
        error_exit("timerfd_create()");
    }
    
    // associate the timer with the epoll instance
    expiration_ctx ctx{ timer.get(), &on_timer_expiration };
    
    struct epoll_event ev;
    ev.events   = EPOLLIN;
    ev.data.ptr = &ctx;
    
    if (epoll_ctl(instance.get(), EPOLL_CTL_ADD, timer.get(), &ev) == -1)
    {
        error_exit("epoll_ctl()");
    }

    // arm the timer
    arm_timer(timer.get(), 2s);

    // monitor for timer completions
    reactor(instance.get(), n_expirations);

    return EXIT_SUCCESS;
}