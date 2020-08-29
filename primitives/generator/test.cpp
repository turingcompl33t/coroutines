// test.cpp
// Basic unit tests for generator<T>.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <stdcoro/coroutine.hpp>
#include <libcoro/generator.hpp>

template <typename T>
coro::generator<T> make_range(T begin, T end)
{
    for (auto i = begin; i < end; ++i)
    {
        co_yield i;
    }
}

// std::accumulate does not support ranges;
// specifically, generator<T> relies on the fact that with ranges,
// the type of the begin and end iterator for a range may differ

template <typename RangeType, typename AccumulationType>
AccumulationType accumulate(RangeType&& range, AccumulationType init)
{
    AccumulationType accumulator{init};
    for (auto value : range)
    {
        accumulator += value;
    }

    return accumulator;
}

TEST_CASE("generators allow us to construct lazy sequences that are range-compatible")
{
    // sum [0, 5)
    auto const sum = accumulate(make_range<int>(0, 5), 0);
    REQUIRE(sum == 10);
}