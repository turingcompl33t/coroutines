// vanilla.cpp

#include <chrono>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <libcoro/nix/unique_fd.hpp>
#include <libcoro/nix/system_error.hpp>

constexpr static auto const DEFAULT_N_EXPIRATIONS = 5ul;

using expiration_callback_f = void (*)(int);

struct expiration_ctx
{
    int                   fd;
    expiration_callback_f cb;
};

template <typename Duration>
void arm_timer(int timer_fd, Duration timeout)
{
    using namespace std::chrono;

    auto const secs = duration_cast<seconds>(timeout);

    // non-periodic timer
    struct itimerspec spec = {
        .it_interval = { 0, 0 },
        .it_value = { secs.count(), 0 }
    };

    int const res = ::timerfd_settime(timer_fd, 0, &spec, nullptr);
    if (-1 == res)
    {
        throw coro::nix::system_error{};
    }
}

void on_timer_expiration(int timer_fd)
{
    using namespace std::chrono_literals;

    puts("[+] timer fired");

    // re-arm the timer for next iteration
    arm_timer(timer_fd, 2s);
}

void reactor(
    int const           epoller, 
    unsigned long const n_expirations)
{
    struct epoll_event ev{};

    for (auto count = 0ul; count < n_expirations; ++count)
    {
        int const n_events = ::epoll_wait(epoller, &ev, 1, -1);
        if (-1 == n_events)
        {
            throw coro::nix::system_error{};
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

    auto const n_expirations = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_EXPIRATIONS;

    // create the epoll instance
    auto instance = coro::nix::unique_fd{::epoll_create1(0)};
    if (!instance)
    {
        throw coro::nix::system_error{};
    }

    // create the timer
    auto timer = coro::nix::unique_fd{::timerfd_create(CLOCK_REALTIME, 0)};
    if (!timer)
    {
        throw coro::nix::system_error{};
    }
    
    // associate the timer with the epoll instance
    expiration_ctx ctx{ timer.get(), &on_timer_expiration };
    
    struct epoll_event ev;
    ev.events   = EPOLLIN;
    ev.data.ptr = &ctx;
    
    if (::epoll_ctl(instance.get(), EPOLL_CTL_ADD, timer.get(), &ev) == -1)
    {
        throw coro::nix::system_error{};
    }

    // arm the timer
    arm_timer(timer.get(), 2s);

    // monitor for timer completions
    reactor(instance.get(), n_expirations);

    return EXIT_SUCCESS;
}