// async_lock.hpp
// An asynchronous lock via coroutines.

#ifndef ASYNC_LOCK_HPP
#define ASYNC_LOCK_HPP

#include <atomic>
#include <cstdint>
#include <utility>
#include <stdcoro/coroutine.hpp>

struct async_lock_awaiter;
struct async_lock_guard;

class async_lock
{
    // The state of the lock.
    std::atomic_uintptr_t state;

    // Implicit queue of awaiters.
    async_lock_awaiter* awaiters;

public:
    async_lock() 
        : state{NOT_LOCKED}
        , awaiters{nullptr} {}

    // non-copyable
    async_lock(async_lock const&)            = delete;
    async_lock& operator=(async_lock const&) = delete;
    
    // non-movable
    async_lock(async_lock&&)            = delete;
    async_lock& operator=(async_lock&&) = delete;

    // Acquire the mutex with guard.
    [[nodiscard]]
    async_lock_awaiter acquire();

private:
    friend class async_lock_guard;
    friend class async_lock_awaiter;

    static constexpr std::uintptr_t NOT_LOCKED        = 0;
    static constexpr std::uintptr_t LOCKED_NO_WAITERS = 1;

    // Release the lock and resume next awaiter, if available.
    void release();
};

struct async_lock_guard
{
    async_lock_guard(async_lock* lock_) 
        : lock{lock_} {}

    ~async_lock_guard()
    {
        // release the lock and resume next awaiter, if present
        if (lock != nullptr)
        {
            lock->release();
        }
    }

    // non-copyable
    async_lock_guard(async_lock_guard const&)            = delete;
    async_lock_guard& operator=(async_lock_guard const&) = delete;

    async_lock_guard(async_lock_guard&& alg)
        : lock{alg.lock}
    {
        alg.lock = nullptr;
    }

    async_lock_guard& operator=(async_lock_guard&& alg)
    {
        if (std::addressof(alg) != this)
        {
            if (lock != nullptr)
            {
                lock->release();
            }

            lock     = alg.lock;
            alg.lock = nullptr;
        }

        return *this;
    }

private:
    async_lock* lock;
};

struct async_lock_awaiter
{
    using coro_handle_type = stdcoro::coroutine_handle<>;

    async_lock_awaiter(async_lock& lock_) 
        : lock{lock_}
        , next_awaiter{nullptr}
        , awaiting_coro{nullptr}
    {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(coro_handle_type awaiting_coro_)
    {
        awaiting_coro = awaiting_coro_;

        auto prev_state = lock.state.load(std::memory_order_acquire);
        for (;;)
        {
            if (async_lock::NOT_LOCKED == prev_state)
            {
                // the lock is currently free; attempt to acquire

                if (lock.state.compare_exchange_weak(
                    prev_state, 
                    async_lock::LOCKED_NO_WAITERS, 
                    std::memory_order_release, 
                    std::memory_order_relaxed))
                {
                    // acquired lock; do not suspend
                    return false;
                }

                // lost the race to acquire lock
            }
            else
            {
                // the lock is currently acquired;
                // attempt to add ourselves to the linked list of awaiters
                next_awaiter = reinterpret_cast<async_lock_awaiter*>(prev_state);
                if (lock.state.compare_exchange_weak(
                    prev_state, 
                    reinterpret_cast<std::uintptr_t>(this), 
                    std::memory_order_release, 
                    std::memory_order_relaxed))
                {
                    // successfully added ourself to the list
                    return true;
                }
            }
        }
    }

    [[nodiscard]]
    async_lock_guard await_resume()
    {
        return async_lock_guard{&lock};
    }

private:
    friend class async_lock;

    // The associated lock for this awaiter.
    async_lock& lock;

    // The next awaiter in the linked-list of awaiters.
    async_lock_awaiter* next_awaiter;

    // The coroutine handle for this awaiter.
    coro_handle_type awaiting_coro;
};


async_lock_awaiter async_lock::acquire()
{
    return async_lock_awaiter{*this};
}

void async_lock::release()
{
    auto* awaiters_head = awaiters;
    if (nullptr == awaiters_head)
    {
        // currently, no awaiters present
        auto prev_state = LOCKED_NO_WAITERS;
        auto const released = state.compare_exchange_strong(
            prev_state,
            NOT_LOCKED,
            std::memory_order_release,
            std::memory_order_relaxed);

        if (released)
        {
            // successfully released the lock with no awaiters; nothing more to do
            return;
        }

        // otherwise, an awaiter snuck in after our initial test of awaiters_head
        prev_state = state.exchange(LOCKED_NO_WAITERS, std::memory_order_acquire);

        // transfer the list ("stack") of awaiters maintained by the `state`
        // member to the queue of awaiters, reversing the stack so that the
        // awaiters are actually resumed in FIFO order on subsequent release()
        auto* current_awaiter = reinterpret_cast<async_lock_awaiter*>(prev_state);
        do
        {
            auto* tmp = current_awaiter->next_awaiter;
            current_awaiter->next_awaiter = awaiters_head;
            awaiters_head = current_awaiter;
            current_awaiter = tmp;
        } while (current_awaiter != nullptr);
    }

    awaiters = awaiters_head->next_awaiter;
    awaiters_head->awaiting_coro.resume();
}

#endif // ASYNC_LOCK_HPP