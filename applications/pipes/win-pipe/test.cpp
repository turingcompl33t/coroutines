// cl /EHsc /nologo /std:c++latest /await test.cpp

#include "pipe.hpp"

#include <chrono>
#include <future>
#include <cstdio>
#include <windows.h>

constexpr static auto const BUFFER_SIZE = 64;

void producer(HANDLE pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    unsigned long n_written{};
    for (;;)
    {
        if (!::WriteFile(pipe, buffer, _countof(buffer), &n_written, NULL))
        {
            printf("[producer] error\n");
            break;
        }
        printf("[producer] wrote %u bytes\n", n_written);
        std::this_thread::sleep_for(3s);
    }
}

void consumer(HANDLE pipe)
{
    std::byte buffer[BUFFER_SIZE];

    unsigned long n_read{};
    for (;;)
    {
        if (!::ReadFile(pipe, buffer, _countof(buffer), &n_read, NULL))
        {
            printf("[consumer] error\n");
            break;
        }

        printf("[consumer] read %u bytes\n", n_read);
    }
}

int main()
{
    HANDLE reader;
    HANDLE writer;

    if (!coro::detail::create_pipe_ex(&reader, &writer, NULL, 0, 0, 0))
    {
        return EXIT_FAILURE;
    }

    auto producer_fut = std::async(std::launch::async, producer, writer);
    auto consumer_fut = std::async(std::launch::async, consumer, reader);

    producer_fut.wait();
    consumer_fut.wait();

    return EXIT_SUCCESS;
}