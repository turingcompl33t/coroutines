// driver_vanilla.cpp
// Using the timer service without coroutine integration.

#include "timer_service.hpp"

#include <chrono>
#include <thread>
#include <string>
#include <future>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

constexpr static auto const DEFAULT_N_REPS = 5ul;

void completion_handler(void* ctx)
{
    auto event = reinterpret_cast<HANDLE>(ctx);
    ::SetEvent(event);
}

void wait_for_n_timers(
    timer_service&      service, 
    unsigned long const n_reps)
{
    using namespace std::chrono_literals;

    auto event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

    for (auto count = 0ul; count < n_reps; ++count)
    {
        // post a timer request to the timer service
        service.post(2s, completion_handler, event);
        
        // wait for notification that timer expired
        ::WaitForSingleObject(event, INFINITE);

        puts("[+] timer fired");

        // prepare for next iteration
        ::ResetEvent(event);
    }

    ::CloseHandle(event);

    // request shutdown of the timer service once we have completed our work
    service.shutdown();
}

int main(int argc, char* argv[])
{
    auto const n_reps = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_REPS;

    timer_service service{1};
    
    // launch an asynchronous worker to wait on system timers
    auto fut = std::async(
        std::launch::async, wait_for_n_timers, std::ref(service), n_reps);

    // run the service until shutdown
    service.run();

    // wait for our asynchronous operation to complete
    fut.wait();

    return EXIT_SUCCESS;
}