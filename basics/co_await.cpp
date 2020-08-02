// co_await.cpp
// Your first coroutine.

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

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = std::coroutine_handle<promise_type>;

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

    void return_void() {}

    void unhandled_exception()
    {
        std::terminate();
    }
};

resumable foo()
{
    std::cout << "foo(): enter\n";
    co_await std::suspend_always{};
    std::cout << "foo() exit\n";
}

int main()
{
    auto res = foo();
    while (res.resume());

    return EXIT_SUCCESS;
}
