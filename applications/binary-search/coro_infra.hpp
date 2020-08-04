// coro_infra.hpp
// Interleaved binary search via hand-crafted state machine.
//
// Adapted from original code by Gor Nishanov.

#ifndef CORO_INFRA_H
#define CORO_INFRA_H

#include <cstdio>
#include <cstdint>
#include <exception>
#include <coroutine>

#include <xmmintrin.h>

struct scheduler_queue
{
    constexpr static int const N = 256;
    
    using coro_handle = std::coroutine_handle<>;

    size_t head = 0;
    size_t tail = 0;

    coro_handle arr[N];

    void push_back(coro_handle h)
    {
        arr[head] = h;
        head = (head + 1) % N;
    }

    coro_handle pop_front()
    {
        auto result = arr[tail];
        tail = (tail + 1) % N;
        return result;
    }

    auto try_pop_front()
    {
        return head != tail ? pop_front() : coro_handle{};
    }

    void run()
    {
        while (auto h = try_pop_front())
        {
            h.resume();
        }
    }
};

inline scheduler_queue scheduler;

template <typename T>
struct prefetch_awaitable
{
    T& value;

    prefetch_awaitable(T& value_) 
        : value{value_} {}

    // no way to query the system to determine if the address is
    // already present in cache, so just (pessimistically) assume
    // that it is not and thus that we have to suspend + prefetch
    bool await_ready()
    {
        return false;
    }

    template <typename Handle>
    auto await_suspend(Handle h)
    {
        // prefetch the desired value
        _mm_prefetch(reinterpret_cast<char const*>(
            std::addressof(value)), _MM_HINT_NTA);
        
        auto& q = scheduler;
        
        // add the suspended coroutine to the scheduler queue
        q.push_back(h);
        
        // return the next handle on scheduler queue
        return q.pop_front();
    }

    T& await_resume()
    {
        return value;
    }
};

template <typename T>
auto prefetch(T& value)
{
    return prefetch_awaitable<T>{value};
}

// thread-caching allocator
struct tcalloc
{
    struct header
    {
        header* next;
        size_t size;
    };

    header* root = nullptr;

    size_t last_size_allocated = 0;
    size_t total               = 0;
    size_t alloc_count         = 0;

    ~tcalloc()
    {
        auto current = root;
        while (current)
        {
            auto next = current->next;
            ::free(current);
            current = next;
        }
    }

    void* alloc(size_t sz)
    {
        if (root && root->size <= sz)
        {
            void* mem = root;
            root = root->next;
            return mem;
        }

        ++alloc_count;
        total += sz;
        last_size_allocated = sz;

        return ::malloc(sz);
    }

    void free(void* p, size_t sz)
    {
        auto new_entry = static_cast<header*>(p);
        new_entry->size = sz;
        new_entry->next = root;
        root = new_entry;
    }

    void stats()
    {
        printf("allocs: %zu total: %zu last size: %zu\n", 
            alloc_count, 
            total, 
            last_size_allocated);
    }
};

inline tcalloc allocator;

struct throttler;

struct root_task
{
    struct promise_type;
    using coro_handle = std::coroutine_handle<promise_type>;

    struct promise_type
    {
        throttler* owner = nullptr;

        // utilize the custom tcalloc allocator
        void* operator new(size_t sz)
        {
            return allocator.alloc(sz);
        }

        // utilize the custom tcalloc allocator
        void operator delete(void* p, size_t sz)
        {
            allocator.free(p, sz);
        }

        root_task get_return_object()
        {
            return root_task{*this};
        }

        auto initial_suspend()
        {
            return std::suspend_always{};
        }

        auto final_suspend()
        {
            return std::suspend_never{};
        }

        void return_void();

        void unhandled_exception() noexcept
        {
            std::terminate();
        }
    };

    // make the throttler the "owner" of this coroutine
    auto set_owner(throttler* owner)
    {
        auto result = handle;
        
        // modify the promise to point to the owning throttler
        handle.promise().owner = owner;
        
        // reset our handle
        handle = nullptr;

        return result;
    }

    // destroy the coroutine if present
    ~root_task()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    root_task(root_task const&) = delete;

    root_task(root_task&& rhs) 
        : handle{rhs.handle}
    {
        rhs.handle = nullptr;
    }

private:
    coro_handle handle;

    root_task(promise_type& p)
        : handle{coro_handle::from_promise(p)} {}
};

struct throttler
{
    size_t limit;

    explicit throttler(size_t limit_) 
        : limit{limit_} {}

    void on_task_done()
    {
        ++limit;
    }

    void spawn(root_task t)
    {
        if (0 == limit)
        {
            scheduler.pop_front().resume();
        }

        // add the handle for the task to the scheduler queue
        auto handle = t.set_owner(this);
        scheduler.push_back(handle);
        
        --limit;
    }

    void run()
    {
        scheduler.run();
    }

    ~throttler()
    {
        run();
    }
};

void root_task::promise_type::return_void()
{
    owner->on_task_done();
}

#endif // CORO_INFRASTRUCTURE_H