// bench.cpp

#include <benchmark/benchmark.h>

#include <stdcoro/coroutine.hpp>
#include <libcoro/generator.hpp>

#include "map.hpp"

template <typename T>
coro::generator<T> make_range(T begin, T end)
{
    for (auto i = begin; i < end; ++i)
    {
        co_yield i;
    }
}

static void BM_sequential_multilookup(benchmark::State& state)
{

}

static void BM_interleaved_multilookup(benchmark::State& state)
{
    
}