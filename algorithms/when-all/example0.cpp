// example0.cpp
// Basic example usage of the when_all() algorithm.

#include <cstdio>
#include <vector>
#include <cstdlib>

#include <stdcoro/coroutine.hpp>

#include <libcoro/task.hpp>

#include "when_all.hpp"
#include "task_scheduler.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

task_scheduler scheduler{};

coro::task<void> baz()
{
    trace("enter");

    co_await defer_on(scheduler);

    trace("exit");
}

coro::task<void> bar()
{
    trace("enter");

    co_await defer_on(scheduler);

    trace("exit");
}

coro::task<void> foo()
{   
    trace("enter");

    std::vector<coro::task<void>> tasks{};
    tasks.push_back(bar());
    tasks.push_back(baz());

    co_await when_all(std::move(tasks));

    trace("exit");
}

int main()
{
    auto t = foo();
    t.resume();

    scheduler.run();

    return EXIT_SUCCESS;
}