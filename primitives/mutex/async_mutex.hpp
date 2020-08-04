// async_mutex.hpp
// An asynchronous mutex using coroutines.

#ifndef ASYNC_MUTEX_H
#define ASYNC_MUTEX_H

#include <atomic>
#include <coroutine>

class async_mutex
{
    mutable std::atomic<void*> state;

public:
    async_mutex() noexcept;

    // non-copyable
    async_mutex(async_mutex const&)            = delete;
    async_mutex& operator=(async_mutex const&) = delete;
    
    // non-movable
    async_mutex(async_mutex&&)            = delete;
    async_mutex& operator=(async_mutex&&) = delete;

    // acquire the mutex
    void acquire() const noexcept;
    
    // release the mutex
    void release() const noexcept;

    bool is_acquired() const noexcept;
    bool is_released() const noexcept;

    // return the awaiter type for the mutex
    struct awaiter;
    awaiter operator co_await() noexcept;
};

struct async_mutex::awaiter
{
    using coro_handle = std::coroutine_handle<>;

    awaiter(async_mutex const& mutex_) 
        : mutex{mutex_}
    {}

    bool await_ready()
    {
        void const* mutex_ptr = &mutex;

        // attempt to take ownership of the mutex;
        // if we are successful, can bypass suspension altogether
        auto const acquired = mutex.state.compare_exchange_strong(
            mutex_ptr, nullptr, std::memory_order_seq_cst);
        return acquired;
    }

    bool await_suspend(coro_handle handle_)
    {
        handle = handle_;

        void const* mutex_addr = &mutex;
        void* head = mutex.state.load(std::memory_order_acquire);
        do
        {
            if (mutex.state.compare_exchange_strong(
                mutex_addr, nullptr, std::memory_order_seq_cst))
            {
                // we have acquired the mutex
                return false;
            }
            
        } while (mutex.state.compare_exchange_weak(
            head, this, std::memory_order_seq_cst));
    }

    void await_resume()
    {

    }

private:
    // The `owning` mutex for this awaiter.
    async_mutex const& mutex;

    // The coroutine handle for this awaiter.
    coro_handle handle;
};

async_mutex::async_mutex() noexcept 
    : state{this}
{
    // initialize the mutex to the `released` state
}

void async_mutex::acquire() const noexcept
{

}

void async_mutex::release() const noexcept
{

}

bool async_mutex::is_acquired() const noexcept
{
    return state.load(std::memory_order_acquire) != this;
}

bool async_mutex::is_released() const noexcept
{
    return state.load(std::memory_order_acquire) == this;
}

async_mutex::awaiter
async_mutex::operator co_await() noexcept
{
    // return an awaiter for the mutex
}

#endif // ASYNC_MUTEX_H