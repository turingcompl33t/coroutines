// timer_service.cpp

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

void timer_queue::push_new_timer(service_awaiter* awaiter)
{
    auto const due_time = awaiter->due_time;

    timers.emplace_back(awaiter, due_time);
    std::push_heap(timers.begin(), timers.end(), due_time_comparator);
}

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
    , timer_thread{::CreateThread(nullptr, 0, timer_thread_main, nullptr, 0, nullptr)}
    , wake_event{::CreateEventW(nullptr, FALSE, FALSE, nullptr)}
    , expiration_event{::CreateWaitableTimerW(nullptr, TRUE, nullptr)}
    , active_timers{}
    , new_awaiters{nullptr}
{
    // ensure all of our OS resources initialized correctly
    if (!port || !timer_thread || !wake_event || !expiration_event)
    {
        throw coro::win::system_error{};
    }
}

timer_service::~timer_service()
{

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

    while ((service.service_state.load(std::memory_order_relaxed) & CLOSED_FLAG) == 0)
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
            // earliest due timer became due
            last_set_waitable_timer = time_point_t::max();
        }

        // reset the waitable timer, if necessary
        if (!service.active_timers.is_empty())
        {
            time_point_t const now = clock::now();
            service.active_timers.pop_due_timers(now, ready_awaiters);
            
            if (!service.active_timers.is_empty())
            {
                // need to reset the awaitable timer to next earliest due time
                auto const earliest_due_time = service.active_timers.earliest_due_time();
                if (earliest_due_time != last_set_waitable_timer)
                {
                    LARGE_INTEGER timeout{};
                    due_time_to_relative_filetime(&timeout, earliest_due_time, now);

                    ::SetWaitableTimer(
                        service.expiration_event.get(), 
                        &timeout, 
                        0, nullptr, 
                        nullptr, FALSE);
                }
            }
        }

        // handle ready awaiters
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

    return EXIT_SUCCESS;
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
    , ref_count{2} {}

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

void service_awaiter::await_resume() {}