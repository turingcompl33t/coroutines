// awaitable.cpp

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <stdcoro/coroutine.hpp>
#include <libcoro/eager_task.hpp>
#include <libcoro/nix/unique_fd.hpp>
#include <libcoro/nix/nix_system_error.hpp>

#include "awaitable_timer.hpp"

constexpr static auto const DEFAULT_N_EXPIRATIONS = 5ul;

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
            throw coro::nix_system_error{};
        }

        if (ev.events & EPOLLIN)
        {
            awaitable_timer::on_timer_expire(ev.data.ptr);
        }
    }
}

coro::eager_task<void> waiter(
    int const           ioc, 
    unsigned long const n_expirations)
{
    using namespace std::chrono_literals;

    awaitable_timer timer{ioc, 1s};

    for (auto count = 0ul; count < n_expirations; ++count)
    {
        co_await timer;

        puts("[+] timer fired");
    }

    co_return;
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    auto const n_expirations = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_EXPIRATIONS;

    // create the epoll instance
    auto instance = coro::unique_fd{::epoll_create1(0)};
    if (!instance)
    {
        throw coro::nix_system_error{};
    }

    auto waiter_res = waiter(instance.get(), n_expirations);

    reactor(instance.get(), n_expirations);

    return EXIT_SUCCESS;
}