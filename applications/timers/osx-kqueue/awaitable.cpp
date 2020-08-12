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
#include <libcoro/nix/system_error.hpp>

#include "awaitable_timer.hpp"

constexpr static auto const DEFAULT_N_REPS = 5ul;

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
    auto ioc = coro::nix::unique_fd{::kqueue()};
    if (!ioc)
    {
        throw coro::nix::system_error{};
    }

    // start the timer
    auto t = waiter(ioc.get(), n_reps);

    // run the reactor
    reactor(ioc.get(), n_reps);

    return EXIT_SUCCESS;
}