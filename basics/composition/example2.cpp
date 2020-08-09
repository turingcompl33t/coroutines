// example2.cpp
// Demonstration of a corrected task implementation
// that manages continuation for coroutines.

#include <cstdio>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include "task_v1.hpp"

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
    trace("enter");

    auto t = coro_0();
    while (t.resume());

    trace("exit");

    return EXIT_SUCCESS;
}