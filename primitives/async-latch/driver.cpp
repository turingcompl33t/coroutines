// driver.cpp

#include <cstdio>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include <libcoro/eager_task.hpp>

#include "async_latch.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

coro::eager_task<void> waiter(async_latch& latch)
{
    trace("enter");
    
    co_await latch;

    trace("exit");
}

int main()
{
    async_latch latch{1};

    auto t = waiter(latch);
    latch.count_down();

    return EXIT_SUCCESS;
}