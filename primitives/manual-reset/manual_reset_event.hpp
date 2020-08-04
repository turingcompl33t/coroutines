// manual_reset_event.hpp
// An asycnchronous manual-reset event.
//
// Adapted from example by Lewis Baker.
// https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await

#ifndef MANUAL_RESET_EVENT_H
#define MANUAL_RESET_EVENT_H

#include <atomic>
#include <coroutine>

class manual_reset_event
{
    mutable std::atomic<void*> state;

public:
    manual_reset_event(bool init) noexcept;

    manual_reset_event(manual_reset_event const&)            = delete;
    manual_reset_event& operator=(manual_reset_event const&) = delete;

    manual_reset_event(manual_reset_event&&)            = delete;
    manual_reset_event& operator=(manual_reset_event&&) = delete;

    bool is_set() const noexcept;

    struct awaiter;
    awaiter operator co_await() const noexcept;

    void set() noexcept;
    void reset() noexcept;
};

struct manual_reset_event::awaiter
{
    using coro_handle = std::coroutine_handle<>;

    awaiter(manual_reset_event const& e)
        : event{e}
    {}

    bool await_ready() const noexcept;

    bool await_suspend(coro_handle handle) noexcept;

    void await_resume() noexcept {};

private:
    // The event on which this awaiter waits. 
    manual_reset_event const& event;

    // A handle to the coroutine frame for this awaiter.
    coro_handle awaiting_coro;

    // The next awaiter in the linked list of awaiters.
    awaiter* next_awaiter;

    friend class manual_reset_event;
};

bool manual_reset_event::awaiter::await_ready() const noexcept
{
    return event.is_set();
}

bool manual_reset_event::awaiter::await_suspend(coro_handle handle) noexcept
{
    void const* set_state = &event;

    awaiting_coro = handle;

    // load the `state` member of the event on which this coro waits;
    // the special value addressof(event) indicates that the event is set,
    // while all other values indicate the event is not set and that the
    // state is the head of a linked list of awaiters
    void* old_val = event.state.load(std::memory_order_acquire);
    do
    {
        if (old_val == set_state)
        {   
            // event is already in the `set` state; don't suspend this coro
            return false;
        }

        next_awaiter = static_cast<awaiter*>(old_val);

        // attempt to link this awaiter into the linked list of awaiters
    } while (!event.state.compare_exchange_weak(
        old_val, 
        this, 
        std::memory_order_acquire, 
        std::memory_order_release));

    // if we reach this point, this coro should indeed suspend
    return true;
}

manual_reset_event::manual_reset_event(bool init) noexcept
    : state{init ? this : nullptr}
{}

bool manual_reset_event::is_set() const noexcept
{
    return state.load(std::memory_order_acquire) == this;
}

void manual_reset_event::set() noexcept
{
    void* old_val = state.exchange(this, std::memory_order_acq_rel);
    if (old_val != this)
    {
        // there are awaiters for the event

        // resume all of the awaiters
        auto* waiter = static_cast<awaiter*>(old_val);
        while (waiter != nullptr)
        {
            auto* next = waiter->next_awaiter;
            waiter->awaiting_coro.resume();
            waiter = next;
        } 
    }
}

void manual_reset_event::reset() noexcept
{
    void* old_val = this;

    // if the event is in the `set` state, reset it to 
    // the `unset` state by exchanging a nullptr into the
    // `state` member, otherwise just leave as is
    state.compare_exchange_strong(old_val, nullptr, std::memory_order_acquire);
}

manual_reset_event::awaiter
manual_reset_event::operator co_await() const noexcept
{
    return awaiter{*this};
}

#endif // MANUAL_RESET_EVENT_H