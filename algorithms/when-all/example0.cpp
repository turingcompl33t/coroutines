// example0.cpp

#include <cstdio>
#include <vector>
#include <cstdlib>
#include <coroutine.hpp>

#include "task.hpp"
#include "when_all.hpp"
#include "task_scheduler.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

task_scheduler scheduler{};

task baz()
{
    trace("enter");

    co_await defer_on(scheduler);

    trace("exit");
}

task bar()
{
    trace("enter");

    co_await defer_on(scheduler);

    trace("exit");
}

task foo()
{   
    trace("enter");

    std::vector<task> tasks{};
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