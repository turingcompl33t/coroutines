// timer_service.hpp

#ifndef TIMER_SERVICE_HPP
#define TIMER_SERVICE_HPP

#include "queue.hpp"
#include "win32_error.hpp"
#include "timer_request.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <cassert>
#include <experimental/coroutine>

#include <windows.h>

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

    // The state of the service.
    std::atomic_bool closed;

    // The total number of threads in reactor loop.
    mutable std::atomic_ulong n_active_reactors;

    // The total number of in-flight timers.
    mutable std::atomic_ulong n_inflight_timers;

public:
    explicit timer_service(
        unsigned long const concurrency = 0)
    : port{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrency)}
    , worker{::CreateThread(nullptr, 0, 
        process_requests, this, 0, nullptr)}
    , requests{} 
    , closed{false}
    , n_active_reactors{0}
    {
        if (NULL == port || NULL == worker)
        {
            throw win32_error{};
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
    void add_active_reactor() const noexcept;

    // Increment the number of active threads within service reactor loop.
    void remove_active_reactor() const noexcept;

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

    // The timer request.
    std::unique_ptr<timer_request> request;

    // The handle to the awaiting coroutine.
    std::experimental::coroutine_handle<> awaiting_coro;

    // Constructor for owned timer objects.
    template <typename Duration>
    awaitable(
        timer_service& service_, 
        Duration       timeout_)
        : posted{false}
        , service{service_} 
        , request{std::make_unique<timer_request>(
            service.port, timeout_, resume_awaiting_coroutine, &awaiting_coro)}
        , awaiting_coro{}
    {}

    // Constructor for non-owned timer objects.
    template <typename Duration>
    awaitable(
        timer_service& service_, 
        HANDLE         timer_, 
        Duration       timeout_)
        : posted{false}
        , service{service_} 
        , request{std::make_unique<timer_request>(
            service.port, timer_, timeout_, resume_awaiting_coroutine, &awaiting_coro)}
        , awaiting_coro{}
    {}

    bool await_ready()
    {
        // deny the request if the service is closed
        return service.closed.load(std::memory_order_release);
    }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coro_)
    {
        // store the handle to the awaiting coroutine
        this->awaiting_coro = awaiting_coro_;

        // submit the request to the timer service
        service.requests.push(request.release(), FALSE);
        this->posted = true;

        service.add_inflight_timer();
    }

    bool await_resume() 
    {
        return this->posted;
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
    if (closed.load(std::memory_order_acquire))
    {
        return false;
    }

    add_inflight_timer();
    
    // construct the timer request 
    auto req = std::make_unique<timer_request>(
        port, timeout, completion_handler, ctx);

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
    if (closed.load(std::memory_order_acquire))
    {
        return false;
    }

    add_inflight_timer();

    // construct the timer request 
    auto req = std::make_unique<timer_request>(
        port, timer, timeout, completion_handler, ctx);

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
    return awaitable{*this, timer, timeout};
}

// The timer service reactor.
void timer_service::run() const
{
    unsigned long bytes_xfer{};
    ULONG_PTR     key{};
    LPOVERLAPPED  pov{};

    if (closed.load(std::memory_order_acquire))
    {
        return;
    }

    add_active_reactor();

    for (;;)
    {
        // wait indefinitely for timer completion
        if (!::GetQueuedCompletionStatus(port, &bytes_xfer, &key, &pov, INFINITE))
        {
            throw win32_error{};
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
        // the timer service and creates a deadlock
        remove_inflight_timer();

        // call the completion handler
        req->completion_handler(req->ctx);

    }  // request destroyed here

    remove_active_reactor();
}

void timer_service::shutdown()
{
    // prevent new requests from entering queue
    closed.store(true, std::memory_order_release);

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

void timer_service::add_active_reactor() const noexcept
{
    n_active_reactors.fetch_add(1, std::memory_order_release);
}

void timer_service::remove_active_reactor() const noexcept
{
    n_active_reactors.fetch_sub(1, std::memory_order_release);
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
    auto* awaiting_coro = static_cast<std::experimental::coroutine_handle<>*>(ctx);
    awaiting_coro->resume();
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
    unsigned long, 
    unsigned long)
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