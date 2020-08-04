// vanilla.cpp
// cl /EHsc /nologo /std:c++latest /await vanilla.cpp

#include "io_context.hpp"
#include "win32_error.hpp"

#include <windows.h>

#include <cstdio>
#include <thread>
#include <string>
#include <cstdlib>

struct timer_context
{
    std::size_t count;
    std::size_t max_count;
    HANDLE      shutdown_handle;
};

template <typename Duration>
void timeout_to_filetime(Duration duration, FILETIME* ft)
{   
    ULARGE_INTEGER tmp{};

    auto const ns = std::chrono::duration_cast<
        std::chrono::nanoseconds>(duration);

    // filetime represented in 100 ns increments
    auto const ns_incs = (ns.count() / 100);

    // lazy way to split the high and low words 
    tmp.QuadPart = -1*ns_incs;

    ft->dwLowDateTime  = tmp.LowPart;
    ft->dwHighDateTime = tmp.HighPart;
}

void __stdcall on_timer_expiration(
    PTP_CALLBACK_INSTANCE instance, 
    void*                 ctx, 
    PTP_TIMER             timer)
{
    using namespace std::chrono_literals;

    puts("[+] timer fired");

    auto& timer_ctx = *reinterpret_cast<timer_context*>(ctx);
    if (++timer_ctx.count >= timer_ctx.max_count)
    {
        // time to shutdown
        ::SetEvent(timer_ctx.shutdown_handle);
    }
    else
    {
        // otherwise, reset the timer

        FILETIME due_time{};
        timeout_to_filetime(2s, &due_time);

        // set the timer object
        ::SetThreadpoolTimer(timer, &due_time, 0, 0);
    }
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    auto const max_count = argc > 1 
        ? std::stoull(argv[1]) 
        : std::size_t{5};

    // create the IO context 
    io_context ioc{1};

    // create the timer context passed to each callback
    timer_context ctx{0, max_count, ioc.shutdown_handle()};

    // create the timer object
    auto timer_obj = ::CreateThreadpoolTimer(
        on_timer_expiration, 
        &ctx, 
        ioc.env());
    if (NULL == timer_obj)
    {
        throw win32_error{};
    }

    FILETIME due_time{};
    timeout_to_filetime(2s, &due_time);

    // set the timer object
    ::SetThreadpoolTimer(timer_obj, &due_time, 0, 0);

    ioc.run();

    return EXIT_SUCCESS;
}