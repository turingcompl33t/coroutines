// awaitable.cpp

#include <string>
#include <chrono>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include <libcoro/eager_task.hpp>
#include <libcoro/win/system_error.hpp>

#include "io_context.hpp"
#include "awaitable_timer.hpp"

constexpr static auto const DEFAULT_N_REPS = 5ul;

coro::eager_task<void> start_timer(
    io_context&         ioc, 
    unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    awaitable_timer two_seconds{ioc, 2s};

    for (auto i = 0ul; i < n_reps; ++i)
    {   
        co_await two_seconds;

        puts("[+] timer fired");
    }

    ioc.shutdown();
}

int main(int argc, char* argv[])
{
    auto const n_reps = argc > 1 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    io_context ioc{1};

    auto t = start_timer(ioc, n_reps);

    ioc.run();
    ioc.wait_close();

    return EXIT_SUCCESS;
}