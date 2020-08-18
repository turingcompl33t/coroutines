// async_latch.hpp

#ifndef ASYNC_LATCH_HPP
#define ASYNC_LATCH_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdcoro/coroutine.hpp>

struct latch_awaiter;

class async_latch
{
public:
    async_latch(std::size_t const count_) 
        : count{count_}
        , awaiters{nullptr}
    {
        assert(count_ > 0);
    }

    ~async_latch() = default;

    // non-copyable
    async_latch(async_latch const&)            = delete;
    async_latch& operator=(async_latch const&) = delete;

    // non-movable
    async_latch(async_latch&&)            = delete;
    async_latch& operator=(async_latch&&) = delete;

    latch_awaiter operator co_await();

    bool expired() const noexcept
    {
        auto const c = count.load(std::memory_order_acquire);
        return (0 == c);
    }

    void count_down(std::size_t const n = 1);

    friend struct latch_awaiter;

private:
    std::size_t get_count() const noexcept
    {
        return count.load(std::memory_order_acquire);
    }

private:
    // The internal count of the latch.
    std::atomic_size_t count;

    std::atomic<latch_awaiter*> awaiters;
};

struct latch_awaiter
{
    // A reference to the latch upon which this awaiter awaits.
    async_latch& latch;

    // A handle to the owning coroutine.
    stdcoro::coroutine_handle<> coro_handle;

    // The next awaiter in linked-list of awaiters, or nullptr.
    latch_awaiter* next_awaiter;

    latch_awaiter(async_latch& latch_) 
        : latch{latch_}
        , coro_handle{}
        , next_awaiter{nullptr}   
    {}

    bool await_ready()
    {
        // The coroutine should proceed immediately if the
        // latch is already expired (count == 0).
        return latch.expired();
    }

    bool await_suspend(stdcoro::coroutine_handle<> coro_handle_)
    {
        coro_handle = coro_handle_;

        auto* old_head = latch.awaiters.load(std::memory_order_acquire);

        do
        {
            if (latch.expired())
            {
                return false;
            }

            next_awaiter = old_head;                

        } while (!latch.awaiters.compare_exchange_weak(
            old_head, 
            this, 
            std::memory_order_acq_rel));

        return true;
    }

    void await_resume() {}
};

latch_awaiter async_latch::operator co_await()
{
    return latch_awaiter{*this};
}

void async_latch::count_down(std::size_t const n)
{   
    auto old_count = count.load(std::memory_order_acquire);
    auto new_count = old_count - n;

    do
    {
        if (n > old_count)
        {
            return;
        }

        new_count = old_count - n;

    } while (!count.compare_exchange_weak(
        old_count, 
        new_count, 
        std::memory_order_acq_rel));

    if (0 == new_count)
    {
        // this operation released the latch
        auto* awaiter = awaiters.load(std::memory_order_acquire);
        while (nullptr != awaiter)
        {
            latch_awaiter* next = awaiter->next_awaiter;
            awaiter->coro_handle.resume();
            awaiter = next;
        }
    }
}

#endif // ASYNC_LATCH_HPP