// driver.cpp

#include <libcoro/task.hpp>
#include <libcoro/task_queue.hpp>

#include <cstdio>
#include <cstdlib>
#include <coroutine>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

coro::task_queue tasks{};

coro::task<void> baz()
{
    trace("enter");

    co_await std::suspend_always{};

    trace("exit");
}

coro::task<void> bar()
{
    trace("enter");

    co_await std::suspend_always{};

    trace("exit");
}

coro::task<void> foo()
{
    trace("enter");

    auto bar_task = bar();
    tasks.schedule(bar_task.handle());
    
    co_await bar_task;

    trace("after co_await bar()");

    auto baz_task = baz();
    tasks.schedule(baz_task.handle());

    co_await baz_task;

    trace("after co_await baz()");

    trace("exit");
}

int main()
{
    auto root = foo();

    tasks.schedule(root.handle());
    tasks.run_all();

    return EXIT_SUCCESS;
}