// example1.cpp
// Demonstration of a broken task implementation
// without support for continuations.

#include "task_v0.hpp"

#include <cstdio>
#include <cstdlib>
#include <coroutine>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

task completes_synchronously()
{
    trace("enter");
    
    trace("exit");

    co_return;
}

task coro_2()
{
    trace("enter");

    co_await completes_synchronously();

    trace("exit");
}

task coro_1()
{
    trace("enter");

    co_await coro_2();

    trace("exit");
}

task coro_0()
{
    trace("enter");

    co_await coro_1();

    trace("exit");
}

int main()
{   
    auto t = coro_0();
    while (t.resume());

    return EXIT_SUCCESS;
}