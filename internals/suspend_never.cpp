// suspend_never.cpp
// Implenting std::suspend_never.

#include "task.hpp"
#include "trace.hpp"

#include <iostream>
#include <coroutine>

struct suspend_never
{
    using coro_handle = std::coroutine_handle<>;

    bool await_ready()
    {
        return true;
    }

    bool await_suspend(coro_handle h)
    {
        return false;
    }

    void await_resume() {}
};

task foo()
{
    trace("enter");

    co_await suspend_never{};

    trace("exit");
}

int main()
{
    trace("enter");
    
    auto t = foo();

    trace("after initial call to foo()");

    while(t.resume());

    return EXIT_SUCCESS;
}