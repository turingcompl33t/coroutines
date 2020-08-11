// driver.cpp

#include <chrono>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include <libcoro/eager_task.hpp>

#include "timer_service.hpp"

constexpr static auto const DEFAULT_N_REPS = 5ul;

coro::eager_task<void> async_waiter(
    timer_service&      service, 
    unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    for (auto i = 0ul; i < n_reps; ++i)
    {
        co_await service.schedule_after(2s);

        puts("[+] timer fired");
    }

    service.shutdown();
}

int main(int argc, char* argv[])
{
    auto const n_reps = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    timer_service service{};

    auto t = async_waiter(service, n_reps);

    service.run();

    return EXIT_SUCCESS;
}

