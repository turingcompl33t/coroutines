// suspend_always.cpp
// Implementing std::suspend_always.

#include "task.hpp"
#include "trace.hpp"

#include <iostream>
#include <coroutine>
#include <stdexcept>

struct suspend_always
{
    using coro_handle = std::coroutine_handle<>;

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(coro_handle h)
    {
        return true;
    }

    void await_resume()
    {}
};

task foo()
{
    trace("enter");

    co_await suspend_always{};

    trace("exit");
}

int main()
{   
    trace("enter");

    auto t = foo();

    trace("after initial call to foo()");

    while (t.resume());

    trace("exit");

    return EXIT_SUCCESS;
}