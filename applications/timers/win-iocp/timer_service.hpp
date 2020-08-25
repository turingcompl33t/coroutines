// timer_service.hpp

#ifndef TIMER_SERVICE_HPP
#define TIMER_SERVICE_HPP

#include "queue.hpp"
#include "timer_request.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <cassert>
#include <windows.h>
#include <stdcoro/coroutine.hpp>

#include <libcoro/win/system_error.hpp>

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

// ----------------------------------------------------------------------------
// timer_service

class timer_service
{
    constexpr static ULONG_PTR const SHUTDOWN_KEY = 1;

    struct awaitable;

    // The native IO completion port that drives the service.
    HANDLE port;

    // Worker thread that processes requests.
    HANDLE worker;

    // Queue of timer requests.
    queue<timer_request> requests;

    // The total number of threads in reactor loop.
    // Bit 0:    closed flag
    // Bit 31-1: count of active reactors
    mutable std::atomic<std::uint32_t> n_active_reactors;

    // The total number of in-flight timers.
    mutable std::atomic<std::uint32_t> n_inflight_timers;

    constexpr static std::uint32_t const CLOSED_FLAG           = 1;
    constexpr static std::uint32_t const NEW_REACTOR_INCREMENT = 2;

public:
    explicit timer_service(
        unsigned long const concurrency = 0)
    : port{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrency)}
    , worker{::CreateThread(nullptr, 0, 
        process_requests, this, 0, nullptr)}
    , requests{} 
    , n_active_reactors{0}
    {
        if (NULL == port || NULL == worker)
        {
            throw coro::win::system_error{};
        }
    }

    ~timer_service()
    {
        ::CloseHandle(worker);
        ::CloseHandle(port);
    }

    // non-copyable
    timer_service(timer_service const&)            = delete;
    timer_service& operator=(timer_service const&) = delete;

    // non-movable (for now...)
    timer_service(timer_service&&)            = delete;
    timer_service& operator=(timer_service&&) = delete;

    // Post a timer request to the service.
    template <typename Duration>
    bool post(
        Duration           timeout, 
        timer_expiration_f completion_handler, 
        void*              ctx);

    // Post a timer request to the service with an existing
    // timer object handle created by CreateWaitableTimer().
    template<typename Duration>
    bool post_with_handle(
        HANDLE             timer, 
        Duration           timeout, 
        timer_expiration_f completion_handler, 
        void*              ctx);

    // Post a timer request to the service, returning an awaitable
    // that resumes the invoking coroutine upon timer expiration.
    template <typename Duration>
    awaitable post_awaitable(Duration timeout);

    // Post a timer request to the service with an existing
    // timer object handle created by CreateWaitableTimer(),
    // returning an awaitable that resumes the invoking coroutine
    // upon timer expiration.
    template <typename Duration>
    awaitable post_awaitable_with_handle(
        HANDLE   timer, 
        Duration timeout);

    // The timer service reactor.
    void run() const;

    // Trigger shutdown of the service, waiting for all outstanding work to complete.
    void shutdown();

private:
    // Increment the number of active threads within service reactor loop.
    bool try_enter_reactor() const noexcept;

    // Increment the number of active threads within service reactor loop.
    void leave_reactor() const noexcept;

    // Increment the in-flight timer count.
    void add_inflight_timer() const noexcept;

    // Decrement the in-flight timer count.
    void remove_inflight_timer() const noexcept;

    // Invoked by reactor thread upon expiration 
    // of a timer submitted by a coroutine.
    static void resume_awaiting_coroutine(void* ctx);

    // The work loop for the service's internal worker thread.
    static unsigned long __stdcall process_requests(void* ctx);

    // The callback function invoked via APC on timer expiration.
    static void on_timer_expiration(
        void* ctx, 
        unsigned long, 
        unsigned long);

    // The callback function invoked via APC on service shutdown.
    static void on_shutdown_request(ULONG_PTR ctx);
};

// ----------------------------------------------------------------------------
// timer_service::awaitable

struct timer_service::awaitable
{
    // Success / failure of the wait operation.
    bool posted;

    // A reference to the owning timer service.
    timer_service& service;

    // The timeout for the associated timer, as a Win32 filetime.
    FILETIME timeout;

    // A user-provided handle to existing timer object.
    HANDLE user_timer;

    // Constructor for owned timer objects.
    template <typename Duration>
    awaitable(
        timer_service& service_, 
        Duration       timeout_)
        : posted{false}
        , service{service_} 
        , timeout{}
        , user_timer{NULL}
    {
        timeout_to_filetime(timeout_, &timeout);
    }

    // Constructor for non-owned timer objects.
    template <typename Duration>
    awaitable(
        timer_service& service_, 
        HANDLE         timer_, 
        Duration       timeout_)
        : posted{false}
        , service{service_} 
        , timeout{}
        , user_timer{timer_}
    {
        timeout_to_filetime(timeout_, &timeout);
    }

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(stdcoro::coroutine_handle<> awaiting_coro)
    {
        // NOTE: cool addition to MSVC with latest release
        // I incorrectly specified the std::memory_order_release
        // memory order for the load operation below while writing
        // this program. Under VS 16.6, the program built and executed
        // just fine despite the fact that this error rendered
        // the program incorrect. Once I updated to VS 16.7, however,
        // this invalid memory order triggered a debug assertion in
        // the standard library, leading me right to the mistake.

        auto const state = service.n_active_reactors.load(std::memory_order_acquire);
        if ((state & CLOSED_FLAG) != 0)
        {
            // deny the request if shutdown has been requested
            return false;
        }

        service.add_inflight_timer();
        posted = true;

        std::unique_ptr<timer_request> request{};
        if (user_timer != NULL)
        {
            // create request with user-provided timer
            request = std::make_unique<timer_request>(
                service.port, timeout, user_timer, resume_awaiting_coroutine, awaiting_coro.address());
        }
        else
        {
            // create a request with a new timer object
            request = std::make_unique<timer_request>(
                service.port, timeout, resume_awaiting_coroutine, awaiting_coro.address());
        }

        // submit the request to the timer service
        service.requests.push(request.release(), FALSE);

        return true;
    }

    bool await_resume() 
    {
        return posted;
    }
};

// ----------------------------------------------------------------------------
// timer_service: Exported

// Post a timer request to the service.
template <typename Duration>
bool timer_service::post(
    Duration           timeout, 
    timer_expiration_f completion_handler, 
    void*              ctx)
{   
    auto const state = n_active_reactors.load(std::memory_order_acquire);
    if ((state & CLOSED_FLAG) != 0)
    {
        return false;
    }

    add_inflight_timer();

    FILETIME as_filetime{};
    timeout_to_filetime(timeout, &as_filetime);
    
    // construct the timer request 
    auto req = std::make_unique<timer_request>(
        port, as_filetime, completion_handler, ctx);

    // post the request to the worker thread
    requests.push(req.release(), FALSE);

    return true;
}

// Post a timer request to the service with an existing
// timer object handle created by CreateWaitableTimer().
template<typename Duration>
bool timer_service::post_with_handle(
    HANDLE             timer, 
    Duration           timeout, 
    timer_expiration_f completion_handler, 
    void*              ctx)
{
    auto const state = n_active_reactors.load(std::memory_order_acquire);
    if ((state & CLOSED_FLAG) != 0)
    {
        return false;
    }


    add_inflight_timer();

    FILETIME as_filetime{};
    timeout_to_filetime(timeout, &as_filetime);

    // construct the timer request 
    auto req = std::make_unique<timer_request>(
        port, as_filetime, timer, completion_handler, ctx);

    // post the request to the worker thread
    requests.push(req.release(), FALSE);

    return true;
}

template <typename Duration>
timer_service::awaitable timer_service::post_awaitable(Duration timeout)
{
    return awaitable{*this, timeout};
}

template <typename Duration>
timer_service::awaitable timer_service::post_awaitable_with_handle(
    HANDLE   timer, 
    Duration timeout)
{
    return awaitable{*this, timeout, timer};
}

// The timer service reactor.
void timer_service::run() const
{
    unsigned long bytes_xfer{};
    ULONG_PTR     key{};
    LPOVERLAPPED  pov{};

    if (!try_enter_reactor())
    {
        return;
    }

    for (;;)
    {
        // wait indefinitely for timer completion
        if (!::GetQueuedCompletionStatus(port, &bytes_xfer, &key, &pov, INFINITE))
        {
            throw coro::win::system_error{};
        }

        if (SHUTDOWN_KEY == key)
        {
            break;
        }

        // extract the request from the posted completion context
        auto req = std::unique_ptr<timer_request>(reinterpret_cast<timer_request*>(pov));

        // decrement the number of in-flight timers
        // NOTE: necessary to perform this operation prior to
        // invoking the completion handler to avoid the case 
        // where the completion handler invokes shutdown() on
        // the timer service and introduces a deadlock
        remove_inflight_timer();

        // call the completion handler
        req->completion_handler(req->ctx);

    }  // request destroyed here

    leave_reactor();
}

void timer_service::shutdown()
{
    // prevent new requests from entering queue
    auto const state = n_active_reactors.fetch_or(CLOSED_FLAG);
    if ((state & CLOSED_FLAG) != 0)
    {
        // shutdown request already submitted
        return;
    }

    // request cancellation of the worker thread
    ::QueueUserAPC(on_shutdown_request, worker, reinterpret_cast<ULONG_PTR>(this));
    ::WaitForSingleObject(worker, INFINITE);

    // finally, break the work loop for all active reactors
    // there is a race here in that we may post strictly more 
    // "shutdown" completions than there are reactors because a
    // reactor may exit the loop between the time the load is
    // performed and the time the completion packet is posted,
    // but this is not an issue for the correctness of the method
    auto const remaining_reactors = n_active_reactors.load(std::memory_order_acquire);
    for (auto i = 0ul; i < remaining_reactors; ++i)
    {
        ::PostQueuedCompletionStatus(port, 0, SHUTDOWN_KEY, nullptr);
    }
}

// ----------------------------------------------------------------------------
// timer_service: Internal

bool timer_service::try_enter_reactor() const noexcept
{
    auto state = n_active_reactors.load(std::memory_order_acquire);
    do
    {
        if ((state & CLOSED_FLAG) != 0)
        {
            return false;
        }
    } while (n_active_reactors.compare_exchange_weak(
        state, 
        state + NEW_REACTOR_INCREMENT, 
        std::memory_order_release));

    return true;
}

void timer_service::leave_reactor() const noexcept
{
    n_active_reactors.fetch_sub(NEW_REACTOR_INCREMENT, std::memory_order_release);
}

void timer_service::add_inflight_timer() const noexcept
{
    n_inflight_timers.fetch_add(1, std::memory_order_release);
}

void timer_service::remove_inflight_timer() const noexcept 
{
    n_inflight_timers.fetch_sub(1, std::memory_order_release);
}

// Invoked by reactor thread upon expiration 
// of a timer submitted by a coroutine.
void timer_service::resume_awaiting_coroutine(void* ctx)
{
    auto awaiting_coro = stdcoro::coroutine_handle<>::from_address(ctx);
    awaiting_coro.resume();
}

// The work loop for the service's internal worker thread.
unsigned long __stdcall timer_service::process_requests(void* ctx)
{
    // retrieve the timer service context
    auto& me = *reinterpret_cast<timer_service*>(ctx);

    for (;;)
    {
        // receive a new timer request;
        // the pop operation blocks in an alertable state, allowing
        // the worker thread to also respond to timer expirations
        auto* req = me.requests.pop(TRUE);

        ::SetWaitableTimer(
            req->timer_object, 
            reinterpret_cast<LARGE_INTEGER*>(&req->timeout), 
            0,
            on_timer_expiration,
            req,
            FALSE);
    }
}

// The callback function invoked via APC on timer expiration.
void timer_service::on_timer_expiration(
    void* ctx, 
    unsigned long /* dwTimerLowValue  */, 
    unsigned long /* dwTimerHighValue */)
{
    // retrieve the timer request context
    auto* req = reinterpret_cast<timer_request*>(ctx);

    // post the timer expiration to the associated completion port
    ::PostQueuedCompletionStatus(
        req->port, 0, 0, reinterpret_cast<LPOVERLAPPED>(req));
}

// The callback function invoked via APC on service shutdown.
void timer_service::on_shutdown_request(ULONG_PTR ctx)
{
    auto& me = *reinterpret_cast<timer_service*>(ctx);

    // clear out any outstanding timers from the queue
    while (auto opt_req = me.requests.try_pop(TRUE))
    {
        auto* req = opt_req.value();
        ::SetWaitableTimer(
            req->timer_object, 
            reinterpret_cast<LARGE_INTEGER*>(&req->timeout), 
            0,
            on_timer_expiration,
            req,
            FALSE);
    }

    // at this point, the queue is no longer accepting new requests,
    // and all further work consists of handling completions for timers
    // that are already in flight, but the worker must remain alive 
    // until all of these completions have been serviced because all of 
    // the timer completion APCs are queued to its APC queue
    while (me.n_inflight_timers.load(std::memory_order_acquire) > 0)
    {
        ::SleepEx(INFINITE, TRUE);
    }

    // now all in-flight timers have been re-posted to the completion port,
    // so it is safe for the worker thread to exit 
    ::ExitThread(EXIT_SUCCESS);
}

#endif // TIMER_SERVICE_HPP