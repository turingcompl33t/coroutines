// driver.cpp
// Example usage of manual_reset_event.

#include <chrono>
#include <cstdio>
#include <vector>
#include <thread>
#include <cstdlib>

#include "task.hpp"
#include "manual_reset_event.hpp"

// "coro.gro may be used unitialized" in consumer()
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

constexpr static auto const N_CONSUMERS = 5;

void producer(manual_reset_event& event)
{
    printf("[Producer] Setting event\n");

    event.set();
}

task consumer(manual_reset_event& event, size_t const id)
{
    printf("[%zu] Enter\n", id);

    co_await event;

    printf("[%zu] Exit\n", id);
}

int main()
{
    using namespace std::chrono_literals;

    manual_reset_event event{false};

    for (size_t i = 0; i < N_CONSUMERS; ++i)
    {
        consumer(event, i);
    }

    producer(event);

    return EXIT_SUCCESS;
}