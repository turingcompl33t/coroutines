// io_context.hpp

#ifndef IO_CONTEXT_HPP
#define IO_CONTEXT_HPP

#include <thread>
#include <windows.h>

#include <libcoro/win/system_error.hpp>

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
            throw coro::win::system_error{};
        }

        ::SetThreadpoolThreadMaximum(pool, concurrency);
        if (!::SetThreadpoolThreadMinimum(pool, concurrency))
        {
            // not enough resources
            throw coro::win::system_error{};
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

    HANDLE const& shutdown_handle() const noexcept
    {
        return shutdown_event;
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
        void*, 
        void*)
    {
        // no-op
    }   
};

#endif // IO_CONTEXT_HPP