// driver2.cpp

#include <libcoro/task.hpp>
#include <libcoro/task_queue.hpp>

#include "pipe.hpp"
#include "io_context.hpp"

#include <cstdio>
#include <chrono>
#include <vector>
#include <thread>
#include <cstdlib>

constexpr static auto const BUFFER_SIZE = 64;

coro::task<void> producer_coro(
    coro::io_context& ioc,
    coro::writeable_pipe&& pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    for (;;)
    {
        auto const n_written = co_await pipe.write_some(buffer, BUFFER_SIZE);
        printf("[producer] wrote %zu bytes\n", n_written);

        std::this_thread::sleep_for(2s);
    }

    co_return;
}

void producer(
    coro::io_context& ioc, 
    coro::writeable_pipe&& pipe)
{
    coro::task_queue tasks{};

    auto root = producer_coro(ioc, std::move(pipe));
    tasks.schedule(root.handle());
    tasks.run_all();
}

coro::task<void> consumer_coro(
    coro::io_context& ioc, 
    coro::readable_pipe&& pipe)
{
    using namespace std::chrono_literals;

    std::byte buffer[BUFFER_SIZE];

    for (;;)
    {
        auto const n_read = co_await pipe.read_some(buffer, BUFFER_SIZE);
        printf("[consumer] read %zu bytes\n", n_read);

        std::this_thread::sleep_for(2s);
    }

    co_return;
}

void consumer(
    coro::io_context& ioc, 
    coro::readable_pipe&& pipe)
{
    coro::task_queue tasks{};

    auto root = consumer_coro(ioc, std::move(pipe));
    tasks.schedule(root.handle());
    tasks.run_all();
}

void reactor(coro::io_context& ioc)
{
    ioc.process_events();
}

int main()
{
    std::vector<std::thread> threads{};

    coro::io_context ioc{16};

    auto [reader, writer] = coro::make_pipe(ioc);

    threads.emplace_back(producer, std::ref(ioc), std::move(writer));
    threads.emplace_back(consumer, std::ref(ioc), std::move(reader));
    threads.emplace_back(reactor, std::ref(ioc));

    for (auto& t : threads)
    {
        t.join();
    }

    return EXIT_SUCCESS;
}