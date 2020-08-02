// driver1.cpp

#include "pipe.hpp"
#include "future.hpp"
#include "io_context.hpp"

#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <cstdlib>

constexpr static auto const BUFFER_SIZE = 64;

void producer(int write_pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    for (;;)
    {
        auto const n_written = ::write(write_pipe, buffer, BUFFER_SIZE);
        printf("[producer] wrote %zu bytes\n", n_written);

        std::this_thread::sleep_for(2s);
    }

    ::close(write_pipe);
}

std::future<void> consumer(
    coro::io_context& ioc, 
    coro::readable_pipe& pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    for (;;)
    {
        auto const n_read = co_await pipe.read_some(buffer, BUFFER_SIZE);
        if (0 == n_read)
        {
            break;
        }

        printf("[consumer] read %zu bytes\n", n_read);
        std::this_thread::sleep_for(1s); 
    }

    co_return;
}

int main()
{
    int pipefds[2];
    
    coro::io_context ioc{8};

    int const res = ::pipe2(pipefds, O_NONBLOCK);
    if (-1 == res)
    {
        return EXIT_FAILURE;
    }

    int const reader = pipefds[0];
    int const writer = pipefds[1];

    printf("pipes (%d, %d)\n", reader, writer);

    coro::readable_pipe read_pipe{reader, ioc};

    // initiate the consumer task
    auto consumer_fut = consumer(ioc, read_pipe);

    // start the producer
    auto producer_fut = std::async(std::launch::async, producer, writer);

    // process IO completion events indefinitely
    ioc.process_events();

    producer_fut.wait();

    return EXIT_SUCCESS;
}