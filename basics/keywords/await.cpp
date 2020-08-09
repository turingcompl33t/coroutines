// await.cpp
// Your first coroutine.

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <stdcoro/coroutine.hpp>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

class resumable
{
public:
    struct promise_type;
    using coro_handle = stdcoro::coroutine_handle<promise_type>;
    
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
    using coro_handle = stdcoro::coroutine_handle<promise_type>;

    resumable get_return_object()
    {
        return coro_handle::from_promise(*this);
    }

    auto initial_suspend()
    {
        return stdcoro::suspend_always{};
    }

    auto final_suspend()
    {
        return stdcoro::suspend_always{};
    }

    void return_void() {}

    void unhandled_exception()
    {
        std::terminate();
    }
};

resumable foo()
{
    trace("enter");

    co_await stdcoro::suspend_always{};

    trace("exit");
}

int main()
{
    auto res = foo();
    while (res.resume());

    return EXIT_SUCCESS;
}
