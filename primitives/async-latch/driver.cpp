// async-latch/driver.cpp

#include <libcoro/task.hpp>
#include <libcoro/task_queue.hpp>
#include <libcoro/async_latch.hpp>

#include <cstdio>
#include <cstdlib>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

coro::task_queue tasks{};

coro::task<void> waiter1(coro::async_latch& latch)
{
    trace("enter");

    co_await latch;

    trace("exit");

    co_return;
}

coro::task<void> waiter2(coro::async_latch& latch)
{
    trace("enter");

    co_await latch;

    trace("exit");

    co_return;
}

coro::task<void> orchestrator(coro::async_latch& latch)
{
    trace("enter");
    
    co_await waiter1(latch);
    co_await waiter2(latch);

    trace("exit");

    co_return;
}

int main()
{
    coro::async_latch latch{1};

    auto root = orchestrator(latch);

    tasks.schedule(root.handle());
    tasks.run_all();

    latch.count_down();

    return EXIT_SUCCESS;
}