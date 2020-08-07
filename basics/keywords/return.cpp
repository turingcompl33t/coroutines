// return.cpp
// Introduction to the co_return keyword.

#include <coroutine.hpp>

#include <cassert>
#include <cstdlib>
#include <iostream>

class resumable
{
public:
    struct promise_type;
    using coro_handle = coro::coroutine_handle<promise_type>;
    
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

    char const* return_val();

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = coro::coroutine_handle<promise_type>;

    char const* string;

    resumable get_return_object()
    {
        return coro_handle::from_promise(*this);
    }

    auto initial_suspend()
    {
        return coro::suspend_always{};
    }

    auto final_suspend()
    {
        return coro::suspend_always{};
    }

    void return_value(char const* string_)
    {   
        string = string_;
    }

    void unhandled_exception()
    {
        std::terminate();
    }
};

char const* resumable::return_val()
{
    return handle.promise().string;
}

resumable foo()
{
    std::cout << "foo(): enter\n";
    co_await coro::suspend_always{};
    std::cout << "foo(): exit\n";
    co_return "hello co_return";
}

int main()
{
    auto res = foo();
    while (res.resume());
    std::cout << res.return_val() << '\n';

    return EXIT_SUCCESS;
}
