// timer_request.hpp

#ifndef TIMER_REQUEST_HPP
#define TIMER_REQUEST_HPP

#include <windows.h>

// The signature of the callback function supplied by 
// application code that will be invoked upon timer expiration.
using timer_expiration_f = void (*)(void*);

template <typename Duration>
static void timeout_to_filetime(Duration duration, FILETIME* ft)
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

// The format of a timer request submitted to the IO context.
struct timer_request
{
    // A (native) handle to the associated completion port.
    HANDLE port;

    // The timer object created by call to CreateWaitableTimer().
    HANDLE timer_object;

    // User provided completion handler.
    timer_expiration_f completion_handler;

    // Arbitrary context passed to user completion handler.
    void* ctx;

    // Is it safe to close the timer handle on request completion?
    bool owned_timer;

    // The desired timeout, as a filetime.
    FILETIME timeout;

    // Construct a new timer request with an owned timer object.
    template <typename Duration>
    timer_request(
        HANDLE             port_,
        Duration           timeout_,
        timer_expiration_f completion_handler_,
        void*              ctx_)
        : port{port_}
        , timer_object{::CreateWaitableTimerA(nullptr, TRUE, nullptr)} 
        , completion_handler{completion_handler_}
        , ctx{ctx_}
        , owned_timer{true}
    {
        // convert the requested timeout to relative filetime
        timeout_to_filetime(timeout_, &timeout);
    }

    // Construct a new timer request with a non-owned timer object.
    template <typename Duration>
    timer_request(
        HANDLE             port_,
        HANDLE             timer_object_,
        Duration           timeout_,
        timer_expiration_f completion_handler_,
        void*              ctx_)
        : port{port_}
        , timer_object{timer_object_}
        , completion_handler{completion_handler_}
        , ctx{ctx_}
        , owned_timer{false}
    {
        // convert the requested timeout to relative filetime
        timeout_to_filetime(timeout_, &timeout);
    }

    ~timer_request()
    {
        // If the application code does not provide a waitable timer
        // handle to the constructor of the request, we internally
        // constructed a new timer object to service the request.
        // Thus, once the request completes, it is safe (and, indeed,
        // necessary to avoid a resource leak) to close the timer handle.

        if (owned_timer)
        {
            ::CloseHandle(timer_object);
        }
    }   

    // non-copyable
    timer_request(timer_request const&)            = delete;
    timer_request& operator=(timer_request const&) = delete;

    // non-movable (for now...)
    timer_request(timer_request&&)            = delete;
    timer_request& operator=(timer_request&&) = delete;
};

#endif // TIMER_REQUEST_HPP