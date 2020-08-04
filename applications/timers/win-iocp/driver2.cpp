// driver2.cpp
//
// cl /EHsc /nologo /std:c++latest /W4 /await driver2.cpp

#include "timer_service.hpp"
#include "eager_resumable.hpp"

#include <chrono>
#include <string>
#include <cstdio>

constexpr static auto const DEFAULT_N_REPS = 5ul;

eager_resumable wait_for_n_timers(
    timer_service&      service, 
    unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    for (auto i = 0ul; i < n_reps; ++i)
    {   
        auto const posted = co_await service.post_awaitable(2s);

        puts("[+] timer fired");
    }

    service.shutdown();

    co_return;
}

int main(int argc, char* argv[])
{
    auto const n_reps = (argc > 1) 
    ? std::stoul(argv[1]) 
    : DEFAULT_N_REPS;

    timer_service service{1};

    // initiate the coroutine
    wait_for_n_timers(service, n_reps);

    // run the timer service reactor
    service.run();

    return EXIT_SUCCESS;
}