// io_context.hpp

#ifndef CORO_IO_CONTEXT_HPP
#define CORO_IO_CONTEXT_HPP

#include "win32_error.hpp"

#include <thread>
#include <windows.h>

class io_context
{ 
    bool closed;

    PTP_POOL          pool;
    PTP_CLEANUP_GROUP cleanup_group;

    TP_CALLBACK_ENVIRON environment;

    HANDLE shutdown_event;

public:
    io_context(unsigned long const concurrency 
        = std::thread::hardware_concurrency())
        : closed{false}
        , pool{::CreateThreadpool(nullptr)}
        , cleanup_group{::CreateThreadpoolCleanupGroup()}
        , shutdown_event{::CreateEventW(nullptr, TRUE, FALSE, nullptr)}
    {
        if (NULL == pool || NULL == cleanup_group)
        {
            throw win32_error{};
        }

        ::SetThreadpoolThreadMaximum(pool, concurrency);
        if (!::SetThreadpoolThreadMinimum(pool, concurrency))
        {
            // not enough resources
            throw win32_error{};
        }

        // initialize a new callback environment
        ::TpInitializeCallbackEnviron(&environment);

        // associate the pool and the cleanup group with the environment
        ::SetThreadpoolCallbackPool(&environment, pool);
        ::SetThreadpoolCallbackCleanupGroup(&environment, cleanup_group, NULL);
    }

    ~io_context()
    {
        wait_close();
        cleanup();
    }

    void run() const noexcept
    {
        ::WaitForSingleObject(shutdown_event, INFINITE);
    }

    PTP_CALLBACK_ENVIRON env() noexcept
    {
        return &environment;
    }

    void shutdown() const noexcept
    {
        ::SetEvent(shutdown_event);
    }

    void wait_close() 
    {
        // blocks until all outstanding callbacks have completed
        ::CloseThreadpoolCleanupGroupMembers(cleanup_group, FALSE, nullptr);
    }

private:
    // cleanup resources
    void cleanup()
    {
        ::CloseHandle(shutdown_event);
        ::CloseThreadpoolCleanupGroup(cleanup_group);
        ::CloseThreadpool(pool);
    }

    static void cleanup_callback(
        void* obj_ctx, 
        void* cleanup_ctx)
    {
        // no-op
    }   
};

#endif // CORO_IO_CONTEXT_HPP