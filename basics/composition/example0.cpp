// example0.cpp
// Basic semantics of the task type.

#include <cstdio>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include "task_v0.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

task async_task()
{
    trace("enter");

    co_await stdcoro::suspend_always{};

    trace("exit");

    co_return;
}

int main()
{
    trace("enter");

    auto t = async_task();

    trace("after call to async_task()");

    t.resume();

    trace("after task::resume() #1");

    t.resume();

    trace("after task::resume #2");

    trace("exit");

    return EXIT_SUCCESS;
}