// test.cpp

#include <cstdio>
#include <vector>
#include <cstdlib>
#include <coroutine.hpp>

#include "task.hpp"
#include "when_all.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

task baz()
{
    trace("enter");

    co_await coro::suspend_always{};

    trace("exit");
}

task bar()
{
    trace("enter");

    co_await coro::suspend_always{};

    trace("exit");
}

task foo()
{   
    trace("enter");

    std::vector<task> tasks{};
    tasks.push_back(bar());
    tasks.push_back(baz());

    co_await when_all(tasks);

    trace("exit");
}

int main()
{
    auto t = foo();
    while (t.resume());

    return EXIT_SUCCESS;
}