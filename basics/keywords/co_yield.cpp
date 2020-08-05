// co_yield.cpp
// Introduction to co_yield.

#include <cstdlib>
#include <iostream>

#include <cassert>
#include <coroutine>

class resumable
{
public:
    struct promise_type;
    using coro_handle = std::coroutine_handle<promise_type>;
    
    resumable(coro_handle handle_) 
        : handle{handle_} {assert(handle);}
    
    ~resumable()
    {
        handle.destroy();
    }

    bool resume()
    {
        if (!handle.done())
        {
            handle.resume();
        }
        return !handle.done();
    }

    const char* recent_val();

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = std::coroutine_handle<promise_type>;

    char const* string = nullptr;

    resumable get_return_object()
    {
        return coro_handle::from_promise(*this);
    }

    auto initial_suspend()
    {
        return std::suspend_always{};
    }

    auto final_suspend()
    {
        return std::suspend_always{};
    }

    auto yield_value(char const* string_)
    {
        string = string_;
        return std::suspend_always{};
    }

    void unhandled_exception()
    {
        std::terminate();
    }
};

const char* resumable::recent_val()
{
    return handle.promise().string;
}

resumable foo()
{
    for (;;)
    {
        co_yield "Hello";
        co_yield "co_yield";
    }
}

int main()
{
    auto res = foo();
    for (size_t i = 0; i < 10; ++i)
    {
        res.resume();
        std::cout << res.recent_val() << '\n';
    }

    return EXIT_SUCCESS;
}
