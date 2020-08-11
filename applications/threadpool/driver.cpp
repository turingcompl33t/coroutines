// driver.cpp

#include <cstdio>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdcoro/coroutine.hpp>

#include <libcoro/eager_task.hpp>

#include "thread_pool.hpp"

constexpr static auto const DEFAULT_N_TASKS = 5ul;

coro::eager_task<void> launch_task(
    thread_pool&        pool, 
    unsigned long const id)
{
    printf("[%zu]: enter\n", id);

    co_await pool.schedule();

    printf("[%zu]: exit\n", id);
}

int main(int argc, char* argv[])
{
    auto const n_tasks = (argc > 1) 
        ? std::stoul(argv[1]) 
        : DEFAULT_N_TASKS;

    thread_pool pool{1};

    std::vector<coro::eager_task<void>> tasks{};
    for (auto i = 0ul; i < n_tasks; ++i)
    {
        tasks.push_back(std::move(launch_task(pool, i)));
    }

    pool.shutdown();

    return EXIT_SUCCESS;
}