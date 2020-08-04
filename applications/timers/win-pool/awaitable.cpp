// awaitable.cpp
// cl /EHsc /nologo /std:c++latest /await awaitable.cpp

#include "io_context.hpp"
#include "win32_error.hpp"
#include "eager_resumable.hpp"
#include "awaitable_timer.hpp"

#include <string>
#include <chrono>
#include <cstdlib>
#include <experimental/coroutine>

constexpr static auto const DEFAULT_N_REPS = 5ul;

eager_resumable start_timer(
    io_context&         ioc, 
    unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    awaitable_timer timer{ioc, 2s};

    for (auto i = 0; i < n_reps; ++i)
    {   
        co_await timer;

        puts("[+] timer fired");
    }

    ioc.shutdown();

    co_return;
}

int main(int argc, char* argv[])
{
    auto const n_reps = argc > 1 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    io_context ioc{1};

    start_timer(ioc, n_reps);

    ioc.run();
    ioc.wait_close();

    return EXIT_SUCCESS;
}