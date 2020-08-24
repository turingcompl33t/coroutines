// bench.cpp

#include "benchmark/benchmark.h"

#include <chrono>
#include <vector>

#include "rng.hpp"
#include "vanilla.hpp"
#include "coroutine.hpp"
#include "state_machine.hpp"

// seed for the random number generator
constexpr static unsigned int const RNG_SEED = 1;

// the number of lookups to perform on the dataset
constexpr static auto const N_LOOKUPS = 1024*1024;

// the bounds for the size of the dataset (count of integer items)
constexpr static auto const MIN_DATASET_SIZE = 200*1024;
constexpr static auto const MAX_DATASET_SIZE = 256*1024*1024;

// the bounds for the number of streams for interleaved multi-lookup
constexpr static auto const MIN_N_STREAMS = 1;
constexpr static auto const MAX_N_STREAMS = 32;

[[nodiscard]]
static std::vector<int> generate_dataset(
    std::size_t const dataset_size)
{
    std::vector<int> dataset{};
    dataset.reserve(dataset_size);
    for (auto i = 0ul; i < dataset_size; ++i)
    {
        dataset.push_back(static_cast<int>(i + i));
    }

    return dataset;
}

[[nodiscard]]
static std::vector<int> generate_lookups(
    std::size_t const  dataset_size, 
    std::size_t const  n_lookups,
    unsigned int const seed)
{
    std::vector<int> lookups{};
    lookups.reserve(n_lookups);
    for (auto i : rng<int>{seed, 0, static_cast<int>(dataset_size*2), n_lookups})
    {
        lookups.push_back(i);
    }

    return lookups;
}

// a vanilla binary search, without multi-lookup support
static void test_vanilla(
    std::vector<int> const& dataset, 
    std::vector<int> const& lookups) 
{    
    auto const beg = dataset.begin();
    auto const end = dataset.end();
    
    // perform a search on the dataset for each key in lookups
    for (int key : lookups)
    {
        vanilla_binary_search(beg, end, key);
    }
}

static void BM_vanilla(benchmark::State& state)
{
    using hr_clock = std::chrono::high_resolution_clock;

    auto const dataset_size = static_cast<std::size_t>(state.range(0));

    auto const dataset = generate_dataset(dataset_size);
    auto const lookups = generate_lookups(dataset_size, N_LOOKUPS, RNG_SEED);

    for (auto _ : state)
    {
        auto const start = hr_clock::now();

        test_vanilla(dataset, lookups);

        auto const stop = hr_clock::now();
        auto const elapsed = std::chrono::duration_cast<
            std::chrono::duration<double>>(stop - start);

        state.SetIterationTime(elapsed.count());
        state.SetItemsProcessed(static_cast<int64_t>(N_LOOKUPS));
    }
}

// a multi-lookup test implemented via hand-crafted state machine
static void test_state_machine(
    std::vector<int> const& dataset, 
    std::vector<int> const& lookups, 
    std::size_t const       n_streams) 
{
    state_machine_multi_lookup(dataset, lookups, n_streams);
}

static void BM_state_machine(benchmark::State& state)
{
    using hr_clock = std::chrono::high_resolution_clock;

    auto const dataset_size = static_cast<std::size_t>(state.range(0));
    auto const n_streams    = static_cast<std::size_t>(state.range(1));

    auto const dataset = generate_dataset(dataset_size);
    auto const lookups = generate_lookups(dataset_size, N_LOOKUPS, RNG_SEED);

    for (auto _ : state)
    {
        auto const start = hr_clock::now();

        test_state_machine(dataset, lookups, n_streams);

        auto const stop = hr_clock::now();
        auto const elapsed = std::chrono::duration_cast<
            std::chrono::duration<double>>(stop - start);

        state.SetIterationTime(elapsed.count());
        state.SetItemsProcessed(static_cast<int64_t>(N_LOOKUPS));
    }
}

// a multi-lookup test implemented via coroutines
static void test_coroutine(
    std::vector<int> const& dataset, 
    std::vector<int> const& lookups, 
    std::size_t const       n_streams)
{
    coro_multi_lookup(dataset, lookups, n_streams);
}

static void BM_coroutine(benchmark::State& state)
{
    using hr_clock = std::chrono::high_resolution_clock;

    auto const dataset_size = static_cast<std::size_t>(state.range(0));
    auto const n_streams    = static_cast<std::size_t>(state.range(1));

    auto const dataset = generate_dataset(dataset_size);
    auto const lookups = generate_lookups(dataset_size, N_LOOKUPS, RNG_SEED);

    for (auto _ : state)
    {
        auto const start = hr_clock::now();

        test_coroutine(dataset, lookups, n_streams);

        auto const stop = hr_clock::now();
        auto const elapsed = std::chrono::duration_cast<
            std::chrono::duration<double>>(stop - start);

        state.SetIterationTime(elapsed.count());
        state.SetItemsProcessed(static_cast<int64_t>(N_LOOKUPS));
    }
}

BENCHMARK(BM_vanilla)->UseManualTime();
BENCHMARK(BM_state_machine)->UseManualTime();
BENCHMARK(BM_coroutine)->UseManualTime();

BENCHMARK_MAIN();