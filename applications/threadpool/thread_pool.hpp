// thread_pool.hpp

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <atomic>
#include <thread>
#include <vector>
#include <cassert>
#include <stdcoro/coroutine.hpp>

#include <pthread.h>

#include <libcoro/nix/unique_fd.hpp>
#include <libcoro/nix/nix_system_error.hpp>

#include "queue.hpp"

class thread_pool
{
    // Bit 0:    closed flag
    // Bit 31-1: count of outstanding awaiters 
    std::atomic_uint32_t pool_state;

    // The number of worker threads allocated to the pool.
    unsigned int const n_threads;

    // Handles to each of the pool's worker threads.
    std::vector<std::thread> threads;

    // Queue of awaiters waiting to be resumed on pool.
    queue awaiters;

    // Denotes whether or not we have joined the threads in the pool.
    bool joined;

public:
    struct pool_awaiter;

    thread_pool() 
        : thread_pool{std::thread::hardware_concurrency()} {}

    explicit thread_pool(unsigned int const n_threads_)
        : pool_state{0}
        , n_threads{n_threads_}
        , threads{}
        , awaiters{}
        , joined{false}
    {
        threads.reserve(n_threads);
        for (auto i = 0u; i < n_threads; ++i)
        {
            threads.emplace_back(work_loop, std::ref(*this));
        }
    }

    ~thread_pool()
    {
        if (!joined)
        {
            join();
        }
    }

    // non-copyable
    thread_pool(thread_pool const&)            = delete;
    thread_pool& operator=(thread_pool const&) = delete;

    // non-movable
    thread_pool(thread_pool&&)            = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    [[nodiscard]]
    pool_awaiter schedule();

    void shutdown();

private:
    // A sentinel value pushed onto the awaiter queue in event of shutdown.
    static constexpr uintptr_t const SENTINEL_AWAITER = 0;

    // Values used to track the current state of the pool.
    static constexpr std::uint32_t const CLOSED_FLAG           = 1;
    static constexpr std::uint32_t const NEW_AWAITER_INCREMENT = 2;

    // The work loop for threadpool workers.
    static void work_loop(thread_pool& pool)
    {
        for (;;)
        {
            auto const raw_awaiter = pool.awaiters.pop();
            if (SENTINEL_AWAITER == raw_awaiter)
            {
                // shutdown this thread
                break;
            }

            auto awaiter = stdcoro::coroutine_handle<>::from_address(reinterpret_cast<void*>(raw_awaiter));
            awaiter.resume();

            pool.notify_awaiter_leave();
        }
    }

    // Attempt to "enter" the pool by incrementing the work count.
    bool try_awaiter_enter()
    {
        auto state = pool_state.load(std::memory_order_relaxed);
        do
        {
            if ((state & CLOSED_FLAG) != 0)
            {
                return false;
            }
        } while (!pool_state.compare_exchange_weak(
            state, 
            state + NEW_AWAITER_INCREMENT, 
            std::memory_order_relaxed));

        return true;
    }

    // Notify the pool that awaiter has left 
    // the pool by decrementing the work count.
    void notify_awaiter_leave()
    {
        pool_state.fetch_sub(
            NEW_AWAITER_INCREMENT, 
            std::memory_order_relaxed);
    }

    // Join all of the threads in the pool.
    void join()
    {
        for (auto& t : threads)
        {
            t.join();
        }

        joined = true;
    }

    friend class pool_awaiter;
};

struct thread_pool::pool_awaiter
{
    thread_pool& pool;

    explicit pool_awaiter(thread_pool& pool_)
        : pool{pool_} {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(stdcoro::coroutine_handle<> awaiter)
    {
        if (!pool.try_awaiter_enter())
        {
            // pool is closed
            return false;
        }

        // successfully entered; register the awaiter for resumption on the pool
        auto const raw_awaiter = reinterpret_cast<uintptr_t>(awaiter.address());
        pool.awaiters.push(raw_awaiter); 

        return true;
    }

    void await_resume() {}
};

// schedule the calling coroutine for resumption on the threadpool
thread_pool::pool_awaiter thread_pool::schedule()
{
    return pool_awaiter{*this};
}

// close the pool to further work requests and wait for outstanding work to complete
void thread_pool::shutdown()
{   
    // prevent new awaiters from entering the pool
    auto const old_state = pool_state.fetch_or(CLOSED_FLAG, std::memory_order_relaxed);
    if ((old_state & CLOSED_FLAG) == 0)
    {
        // enqueue a sentinel awaiter for each active pool thread
        std::vector<uintptr_t> sentinels(n_threads, SENTINEL_AWAITER);
        awaiters.push_batch(sentinels);
    }

    join();
}

#endif // THREAD_POOL_HPP