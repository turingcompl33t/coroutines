// driver.cpp
// Example usage of async_lock.

#include <cstdio>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include <libcoro/eager_task.hpp>

#include "async_lock.hpp"

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

coro::eager_task<void> uncontended(async_lock& lock)
{
    trace("enter");

    {
        auto guard = co_await lock.acquire();

        // do things protected by the lock
    }

    trace("exit");
}

int main()
{
    async_lock lock{};

    auto t = uncontended(lock);

    return EXIT_SUCCESS;
}