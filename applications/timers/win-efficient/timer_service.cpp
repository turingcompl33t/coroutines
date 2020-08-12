// timer_service.cpp
// An IO service that supports efficient management of awaitable timers.
// 
// The internal implementation for the timer thread is adapted from 
// the io_service timer management implementation from cppcoro:
// https://github.com/lewissbaker/cppcoro

#include "timer_service.hpp"

#include <thread>
#include <vector>
#include <algorithm>

#include <libcoro/win/system_error.hpp>

// ----------------------------------------------------------------------------
// utility

void due_time_to_relative_filetime(
    LARGE_INTEGER* ft, 
    std::chrono::time_point<
        std::chrono::high_resolution_clock> due_time,
    std::chrono::time_point<
        std::chrono::high_resolution_clock> now)
{
    using namespace std::chrono;

    auto const delta = due_time - now;
    auto const ticks = duration_cast<nanoseconds>(delta).count() / 100;
    ft->QuadPart = -(ticks);
}

// ----------------------------------------------------------------------------
// timer_queue

timer_queue::timer_queue() : timers{} {}

// Push a new timer onto the timer queue.
void timer_queue::push_new_timer(service_awaiter* awaiter)
{
    auto const due_time = awaiter->due_time;

    timers.emplace_back(awaiter, due_time);
    std::push_heap(timers.begin(), timers.end(), due_time_comparator);
}

// Remove all due times from the timer queue onto the output list. 
void timer_queue::pop_due_timers(
    time_point_t      now, 
    service_awaiter*& awaiters)
{
    while (!timers.empty() && timers.front().due_time <= now)
    {
        auto* awaiter = timers.front().awaiter;
        std::pop_heap(timers.begin(), timers.end(), due_time_comparator);
        timers.pop_back();

        // link the due awaiter onto the output list
        awaiter->next = awaiters;
        awaiters      = awaiter;
    }
}

timer_queue::time_point_t timer_queue::earliest_due_time()
{
    return timers.empty() 
        ? time_point_t::max() 
        : timers.front().due_time;
}

service_awaiter* timer_queue::pop_any()
{
    if (timers.empty())
    {
        return nullptr;
    }
    else
    {
        auto* awaiter = timers.back().awaiter;
        timers.pop_back();
        return awaiter;
    }
}

bool timer_queue::is_empty() const noexcept
{
    return timers.empty();
}

bool timer_queue::due_time_comparator(timer_entry const& x, timer_entry const& y)
{
    // the STL heap functions are designed to construct a max heap and specify
    // that the comparator should return `true` if item x is less than item y;
    // in our context, an entry has a higher priority (is "greater") if it has
    // a due time that is earlier than the other entry, so entry x should be ordered
    // after entry y in the queue if it has a later due time than entry y
    return x.due_time > y.due_time;
}

// ----------------------------------------------------------------------------
// timer_service (exported)

timer_service::timer_service()
    : timer_service(std::thread::hardware_concurrency()) {}

timer_service::timer_service(unsigned int const max_threads)
    : port{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, max_threads)} 
    , timer_thread{}
    , wake_event{::CreateEventW(nullptr, FALSE, FALSE, nullptr)}
    , expiration_event{::CreateWaitableTimerW(nullptr, TRUE, nullptr)}
    , active_timers{}
    , new_awaiters{nullptr}
{
    // ensure all of our OS resources initialized correctly
    if (!port || !wake_event || !expiration_event)
    {
        throw coro::win::system_error{};
    }

    // need to ensure that both wake event and expiration event are successfully
    // created prior to launch of the timer thread in order to avoid race
    timer_thread.reset(
        ::CreateThread(nullptr, 0, timer_thread_main, this, 0, nullptr));

    if (!timer_thread)
    {
        throw coro::win::system_error{};
    }
}

void timer_service::run()
{
    ULONG_PTR     key;
    LPOVERLAPPED  pov;
    unsigned long bytes_xfer;

    if (!try_enter_service())
    {
        return;
    }

    for (;;)
    {
        auto const success 
            = ::GetQueuedCompletionStatus(port.get(), &bytes_xfer, &key, &pov, INFINITE);
        if (!success)
        {
            // QUESTION: what to do here..?
        }

        if (SENTINEL_COMPLETION_KEY == key)
        {
            // shutdown request
            break;
        }

        // the coroutine handle for the awaiter is marshalled via the completion key
        auto awaiter_handle 
            = stdcoro::coroutine_handle<>::from_address(reinterpret_cast<void*>(key));
        awaiter_handle.resume();
    }

    leave_service();
}

void timer_service::shutdown()
{
    auto const prev_state = service_state.fetch_or(CLOSED_FLAG, std::memory_order_relaxed);
    if ((prev_state & CLOSED_FLAG) == 0)
    {
        wake_timer_thread();
        ::WaitForSingleObject(timer_thread.get(), INFINITE);

        for (auto i = 0u; i < prev_state; ++i)
        {
            ::PostQueuedCompletionStatus(
                port.get(), 0, SENTINEL_COMPLETION_KEY, nullptr);
        }
    }
}

// ----------------------------------------------------------------------------
// timer_service (internal)

void timer_service::wake_timer_thread()
{
    ::SetEvent(wake_event.get());
}

void timer_service::schedule_awaiter(stdcoro::coroutine_handle<> awaiter_handle)
{
    auto const raw_awaiter = reinterpret_cast<ULONG_PTR>(awaiter_handle.address());
    ::PostQueuedCompletionStatus(port.get(), 0, raw_awaiter, nullptr);
}

unsigned long __stdcall timer_service::timer_thread_main(void* arg)
{
    using clock        = std::chrono::high_resolution_clock;
    using time_point_t = clock::time_point;

    auto& service = *reinterpret_cast<timer_service*>(arg);

    HANDLE timer_thread_events[] = {
        service.wake_event.get(),
        service.expiration_event.get()
    };

    constexpr unsigned long const wake_ev_idx   = 0;
    constexpr unsigned long const expire_ev_idx = 1;

    // The head of a linked-list of awaiters that are due to resume.
    service_awaiter* ready_awaiters = nullptr;

    time_point_t last_set_waitable_timer = time_point_t::max();

    while (!service.is_closed())
    {
        // wait on expiration of the earliest due time and
        // any wake events which result from two conditions:
        //  - shutdown request
        //  - enqueueing of new timer(s)

        auto const result = ::WaitForMultipleObjects(
            _countof(timer_thread_events), 
            timer_thread_events, 
            FALSE, INFINITE);

        if (WAIT_FAILED == result)
        {
            // QUESTION: what to do here..?
        }

        // determine if event signals timer expiration or wake request
        auto const event_index = result - WAIT_OBJECT_0;
        if (event_index == wake_ev_idx)
        {
            // wake request; handle new timers
            auto* new_timer = service.new_awaiters.exchange(nullptr, std::memory_order_acquire);
            while (new_timer != nullptr)
            {
                auto* timer = new_timer;
                new_timer = timer->next;
                service.active_timers.push_new_timer(timer);
            }
        }
        else if (event_index == expire_ev_idx)
        {
            // earliest due timer became due; we set this placeholder here to 
            // handle the case in which we pop all timers from the active timers 
            // queue and then don't subsequently reset the awaitable timer; 
            // it serves as a sort of sentinel value for the last set time
            last_set_waitable_timer = time_point_t::max();
        }

        if (!service.active_timers.is_empty())
        {
            time_point_t const now = clock::now();

            // dequeue due timers, if any, onto temporary list of ready awaiters
            service.active_timers.pop_due_timers(now, ready_awaiters);
            
            // reset the waitable timer to the earliest due time in the timer queue, if any remain
            last_set_waitable_timer = timer_thread_reset_waitable_timer(
                service, last_set_waitable_timer, now);
        }

        // finally, handle ready awaiters by posting 
        // their readiness to the main timer service reactor
        while (ready_awaiters != nullptr)
        {
            auto* awaiter = ready_awaiters;
            ready_awaiters = awaiter->next;

            if (awaiter->ref_count.fetch_sub(1, std::memory_order_release) == 1)
            {
                service.schedule_awaiter(awaiter->coro_handle);
            }
        }
    }

    // NOTE: we drop out of the timer thread immediately on a shutdown request;
    // it is entirely possible that at this point there are coroutines that have
    // scheduled themselves for resumption but the timeout has not yet become 
    // due so they are currently suspended in the timer queue; there are various
    // ways one might go about handling this situation; here, we simply mark each
    // awaiter as "cancelled" and immediately post their resumption to the timer 
    // service; this way, they will ultimately resume but will throw to denote 
    // that the timeout that was expected was not satisfied
    while (auto* awaiter = service.active_timers.pop_any())
    {
        awaiter->cancelled = true;
        service.schedule_awaiter(awaiter->coro_handle);
    }

    return EXIT_SUCCESS;
}

// Reset the waitable timer for the timer thread to earliest due time in the timer queue.
timer_service::time_point_t timer_service::timer_thread_reset_waitable_timer(
    timer_service& service, 
    time_point_t   last_set_time,
    time_point_t   now)
{
    if (!service.active_timers.is_empty())
    {
        auto const earliest_due_time = service.active_timers.earliest_due_time();
        if (earliest_due_time != last_set_time)
        {
            LARGE_INTEGER timeout{};
            due_time_to_relative_filetime(&timeout, earliest_due_time, now);

            ::SetWaitableTimer(
                service.expiration_event.get(), 
                &timeout, 
                0, nullptr, 
                nullptr, FALSE);

            return earliest_due_time;
        }
    }

    return time_point_t::max();
}

bool timer_service::is_closed() const noexcept
{
    return (service_state.load(std::memory_order_relaxed) & CLOSED_FLAG) == 0;
}

bool timer_service::try_enter_service()
{
    auto prev_state = service_state.load(std::memory_order_relaxed);
    do 
    {
        if ((prev_state & CLOSED_FLAG) != 0)
        {
            return false;
        }
    } while (service_state.compare_exchange_weak(
        prev_state, 
        prev_state + NEW_THREAD_INCREMENT, 
        std::memory_order_relaxed));

    return true;
}

void timer_service::leave_service()
{
    service_state.fetch_sub(NEW_THREAD_INCREMENT, std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// service_awaiter

service_awaiter::service_awaiter(
    timer_service& service_, 
    time_point_t due_time_)
    : service{service_}
    , due_time{due_time_}
    , coro_handle{nullptr}
    , next{nullptr}
    , ref_count{2} 
    , cancelled{false} {}

bool service_awaiter::await_ready()
{
    return false;
}

void service_awaiter::await_suspend(
    stdcoro::coroutine_handle<> awaiter)
{
    coro_handle = awaiter;

    // link the new timer into the list of new entries
    auto* prev_head = service.new_awaiters.load(std::memory_order_acquire);
    do 
    {
        next = prev_head;
    } while (!service.new_awaiters.compare_exchange_weak(
        prev_head, 
        this, 
        std::memory_order_release, 
        std::memory_order_acquire));

    if (nullptr == prev_head)
    {
        // the new awaiter that we just inserted is the first in the list;
        // the timer thread needs to be notified that a new awaiter has arrived
        service.wake_timer_thread();
    }

    if (ref_count.fetch_sub(1, std::memory_order_release) == 1)
    {
        // the timer for this awaiter has already expired;
        // schedule this coroutine for immediate resumption on service thread
        service.schedule_awaiter(awaiter);
    }
}

void service_awaiter::await_resume() 
{
    if (cancelled)
    {
        throw timer_cancelled_error{};
    }
}