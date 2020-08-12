// vanilla.cpp
// Basics of the kqueue API.

#include <chrono>
#include <cstdio>
#include <string>
#include <cstdlib>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <libcoro/nix/unique_fd.hpp>
#include <libcoro/nix/system_error.hpp>

// enable use of designated initializers
#pragma clang diagnostic ignored "-Wc99-extensions"

constexpr static auto const DEFAULT_N_REPS = 5ul;

// register a new timer with the kqueue instance
template <typename Duration>
void register_timer(
    int       instance, 
    uintptr_t ident, 
    Duration  timeout)
{
    using namespace std::chrono;

    auto const ms = duration_cast<microseconds>(timeout);
    
    // NOTE: interestingly, the macro NOTE_MSECONDS
    // is not available on OSX despite its appearance in
    // the documentation for other systems that support 
    // kqueue (e.g. BSD) so we need to select either 
    // NOTE_SECONDS to specify the timeout at second-level
    // granulatity, or go all the way down to microseconds 
    // to specify at microsecond-level granularity; annoying

    // NOTE: the default behavior for timers under kqueue
    // is to be periodic, meaning that when we create this
    // timer with the specified timeout value, it will fire
    // every <timeout> microseconds until we disable it

    struct kevent ev = {
        .ident  = ident,
        .filter = EVFILT_TIMER,
        .flags  = EV_ADD | EV_ENABLE,
        .fflags = NOTE_USECONDS,
        .data   = ms.count(),
        .udata  = nullptr
    };

    int const result = ::kevent(instance, &ev, 1, nullptr, 0, nullptr);
    if (-1 == result)
    {
        throw coro::nix::system_error{};
    }
}

// unregister an existing timer with the kqueue instance
void unregister_timer(int instance, uintptr_t ident)
{
    struct kevent ev = {
        .ident  = ident,
        .filter = EVFILT_TIMER,
        .flags  = EV_DISABLE | EV_DELETE,
        .fflags = 0,
        .data   = 0,
        .udata  = nullptr
    };

    int const result = ::kevent(instance, &ev, 1, nullptr, 0, nullptr);
    if (-1 == result)
    {
        throw coro::nix::system_error{};
    }
}

void reactor(
    int                 instance, 
    uintptr_t const     ident, 
    const unsigned long n_reps)
{
    struct kevent ev{};

    for (auto i = 0ul; i < n_reps; ++i)
    {
        int const n_events = ::kevent(instance, nullptr, 0, &ev, 1, nullptr);
        if (-1 == n_events)
        {
            puts("[-] kevent() error");
            break;
        }

        if (ident == ev.ident)
        {
            puts("[+] timer fired");
        }

        // something strange happened...
    }
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    auto const n_reps = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    // create the kqueue instance
    auto instance = coro::nix::unique_fd{::kqueue()};
    if (!instance)
    {
        throw coro::nix::system_error{};
    }

    // the identifier for our timer
    uintptr_t const ident = 0;

    // register a new timer with the kqueue instance
    register_timer(instance.get(), ident, 3s);

    // run the reactor for n_reps iterations
    reactor(instance.get(), ident, n_reps);

    // close up shop
    unregister_timer(instance.get(), ident);

    return EXIT_SUCCESS;
}