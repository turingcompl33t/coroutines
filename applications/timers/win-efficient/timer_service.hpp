// timer_service.hpp
// An IO service that supports efficient management of awaitable timers.

#ifndef TIMER_SERVICE_HPP
#define TIMER_SERVICE_HPP

#include <chrono>
#include <atomic>
#include <vector>
#include <thread>
#include <stdcoro/coroutine.hpp>

#include <libcoro/win/unique_handle.hpp>

// ----------------------------------------------------------------------------
// timer_queue

class timer_service;
struct service_awaiter;

class timer_queue
{
    using time_point_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    // An individual entry in the timer queue.
    struct timer_entry
    {
        // The address of the awaiter as an integral type.
        service_awaiter* awaiter;

        // The time at which this awaiter should be resumed.
        time_point_t due_time;

        timer_entry(service_awaiter* awaiter_, time_point_t due_time_)
            : awaiter{awaiter_}
            , due_time{due_time_} {}
    };

    // A sorted container of timer entries.
    std::vector<timer_entry> timers;

public:
    timer_queue();

    ~timer_queue() = default;

    // non-copyable
    timer_queue(timer_queue const&)            = delete;
    timer_queue& operator=(timer_queue const&) = delete;

    // non-movable
    timer_queue(timer_queue&&)            = delete;
    timer_queue& operator=(timer_queue&&) = delete;

    void push_new_timer(service_awaiter* awaiter);

    void pop_due_timers(
        time_point_t      now, 
        service_awaiter*& awaiters);

    time_point_t earliest_due_time();

    service_awaiter* pop_any();

    bool is_empty() const noexcept;

    static bool due_time_comparator(
        timer_entry const& x, 
        timer_entry const& y);
};

// ----------------------------------------------------------------------------
// timer_service

class timer_service
{
    using time_point_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    // Bit 0:    closed flag
    // Bit 31-1: number of threads running service
    std::atomic_uint32_t service_state;

    // Native IOCP handle.
    coro::win::null_handle port;

    // Native handle for the internal timer thread.
    coro::win::null_handle timer_thread;

    // Events managed by the internal timer thread.
    coro::win::null_handle wake_event;
    coro::win::null_handle expiration_event;

    // The sorted container of timers.
    timer_queue active_timers;

    // The head of the singly-linked list of timer entries.
    // Used as interim storage between the point at which a
    // couroutine submits a new timer and the timer thread wakes.
    std::atomic<service_awaiter*> new_awaiters;

public:
    timer_service();

    explicit timer_service(unsigned int const max_threads);

    ~timer_service() = default;

    // non-copyable
    timer_service(timer_service const&)            = delete;
    timer_service& operator=(timer_service const&) = delete;

    // non-movable 
    timer_service(timer_service&&)            = delete;
    timer_service& operator=(timer_service&&) = delete;

    // Process completions on the timer service.
    void run();

    // Request shutdown of the timer service.
    void shutdown();

    // Schedule resumption of a coroutine after specified duration.
    template <typename Duration>
    [[nodiscard]] service_awaiter schedule_after(Duration delay);

private:
    // Sentinel completion key used to indicate shutdown request.
    static constexpr ULONG_PTR const SENTINEL_COMPLETION_KEY = 0;

    static constexpr std::uint32_t const CLOSED_FLAG          = 1;
    static constexpr std::uint32_t const NEW_THREAD_INCREMENT = 2;

    void wake_timer_thread();

    void schedule_awaiter(stdcoro::coroutine_handle<> awaiter_handle);

    static unsigned long __stdcall timer_thread_main(void* arg);
    
    static time_point_t timer_thread_reset_waitable_timer(
        timer_service& service, 
        time_point_t   last_set_time,
        time_point_t   now);

    bool is_closed() const noexcept;

    bool try_enter_service();
    void leave_service();

    friend class  timer_entry;
    friend class  timer_queue;
    friend struct service_awaiter;
};

template <typename Duration>
[[nodiscard]] service_awaiter 
timer_service::schedule_after(Duration delay)
{
    auto due_time = std::chrono::high_resolution_clock::now() + delay;
    return service_awaiter{*this, due_time};
}

// ----------------------------------------------------------------------------
// service_awaiter

struct service_awaiter
{
    using time_point_t = timer_service::time_point_t;

    timer_service& service;
    time_point_t   due_time;

    stdcoro::coroutine_handle<> coro_handle;

    service_awaiter* next;

    std::atomic_size_t ref_count;

    bool cancelled;

    service_awaiter(
        timer_service& service_, 
        time_point_t   due_time_);

    bool await_ready();

    void await_suspend(stdcoro::coroutine_handle<> awaiter);

    void await_resume();
};

// ----------------------------------------------------------------------------
// timer_cancelled_error

struct timer_cancelled_error {};

#endif // TIMER_SERVICE_HPP