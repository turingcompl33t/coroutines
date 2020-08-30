// bench_interleaved.cpp

#include <benchmark/benchmark.h>

#include <chrono>
#include <string>
#include <stdcoro/coroutine.hpp>
#include <libcoro/generator.hpp>

#include "map.hpp"
#include "scheduler.hpp"
#include "dev_null_iterator.hpp"

// the maximum capacity of the map instance 
constexpr static std::size_t const MAP_MAX_CAPACITY = 1 << 16;

// the number of concurrent instruction streams used in multilookup;
// after running this benchmark varying the number of streams, I saw 
// very little difference between runs depedendent on the stream count, 
// varying the number of streams used between 8 and 32
constexpr static std::size_t const N_STREAMS = 10;

// the upper and lower bound on the number of items in the map;
// also the number of lookups that we perform in this test iteration
constexpr static std::size_t const MIN_N_ITEMS = 1 << 16;  // ~65,000 keys
constexpr static std::size_t const MAX_N_ITEMS = 1 << 25;  // ~32 million keys

// the upper and lower bound on the number of simultaneous
// instruction streams to maintain for ISI with coroutines
constexpr static std::size_t const MIN_N_STREAMS =  8;
constexpr static std::size_t const MAX_N_STREAMS = 32;

// the size of the temporary buffer for generating messages
constexpr static std::size_t MSG_BUFFER_SIZE = 64;

template <typename T>
coro::generator<T> make_range(T begin, T end)
{
    for (auto i = begin; i < end; ++i)
    {
        co_yield i;
    }
}

static void BM_interleaved_multilookup(benchmark::State& state)
{
    using hr_clock = std::chrono::high_resolution_clock;

    auto const n_items   = static_cast<std::size_t>(state.range(0));

    for (auto _ : state)
    {
        // construct the scheduler for scheduling coroutines; depth is immaterial
        StaticQueueScheduler<32> scheduler{};

        // construct the map and insert `n_items` elements
        Map<int, int> map{MAP_MAX_CAPACITY};
        for (auto i : make_range<int>(0, static_cast<int>(n_items)))
        {
            map.insert(i, i);
        }

        // create a lazy lookup range; 
        // we don't pay memory cost of a massive e.g. vector with all of the lookup keys
        auto lookup_range = make_range<int>(0, static_cast<int>(n_items));

        // output iterator is a no-op;
        // we don't pay for e.g. a std::vector::push_back() on each iteration
        DevNullIterator output_iter{};

        auto const start = hr_clock::now();

        map.interleaved_multilookup(
            lookup_range.begin(), 
            lookup_range.end(), 
            output_iter,
            scheduler, 
            N_STREAMS);

        auto const stop = hr_clock::now();
        auto const as_double = std::chrono::duration_cast<
            std::chrono::duration<double>>(stop - start);

        state.SetIterationTime(as_double.count());

        auto const as_ns = std::chrono::duration_cast<
            std::chrono::nanoseconds>(stop - start);

        auto const ns_per_lookup = as_ns.count() / static_cast<long int>(n_items);

        char message[MSG_BUFFER_SIZE];
        ::snprintf(message, sizeof(message), 
            "%zu items: %zu ns per lookup", n_items, ns_per_lookup);

        state.SetLabel(message);
    }
}

BENCHMARK(BM_interleaved_multilookup)
    ->RangeMultiplier(2)
    ->Range(MIN_N_ITEMS, MAX_N_ITEMS)
    ->UseManualTime();

BENCHMARK_MAIN();