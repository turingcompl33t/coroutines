// driver.cpp
// cl /EHsc /nologo /std:c++latest /await driver.cpp 

#include "pipe.hpp"
#include "io_context.hpp"

#include <cstdio>
#include <chrono>
#include <future>
#include <cstdlib>
#include <experimental/coroutine>

#include <windows.h>

constexpr static auto const BUFFER_SIZE = 64;

void producer(HANDLE write_pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    unsigned long n_written{};
    for (;;)
    {
        if (!::WriteFile(write_pipe, buffer, BUFFER_SIZE, &n_written, NULL))
        {
            break;
        }

        printf("[producer] wrote %u bytes\n", n_written);
        std::this_thread::sleep_for(2s);
    }
}

void consumer(coro::readable_pipe& read_pipe)
{
    using namespace std::chrono_literals;

    OVERLAPPED ov;
    std::byte buffer[BUFFER_SIZE];

    for (;;)
    {   
        //auto const n_read = co_await read_pipe.read_some(buffer, BUFFER_SIZE);
        read_pipe.read(buffer, _countof(buffer), &ov);
        printf("[consumer] issue read\n");
        std::this_thread::sleep_for(3s);
    }
}

void reactor(coro::io_context const& ioc)
{
    ioc.process_events();
}

int main()
{
    HANDLE reader;
    HANDLE writer;

    coro::io_context ioc{};

    if (!coro::detail::create_pipe_ex(
        &reader, 
        &writer, 
        NULL, 
        0, 
        FILE_FLAG_OVERLAPPED, 
        FILE_FLAG_OVERLAPPED))
    {
        return EXIT_FAILURE;
    }

    printf("reader handle = %p\n", reader);

    // create the readable pipe instance
    coro::readable_pipe read_pipe{ioc, reader};

    // launch the consumer coroutine
    auto consumer_fut = std::async(std::launch::async, consumer, std::ref(read_pipe));

    // launch the producer thread
    auto producer_fut = std::async(std::launch::async, producer, writer);

    // process completion events indefinitely
    reactor(ioc);

    producer_fut.wait();

    return EXIT_SUCCESS;
}   