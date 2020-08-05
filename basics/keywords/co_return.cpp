// co_return.cpp
// Introduction to co_return.

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

    char const* return_val();

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = std::coroutine_handle<promise_type>;

    char const* string;

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
    co_await std::suspend_always{};
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
