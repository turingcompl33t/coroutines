// driver.cpp

#include <cstdio>
#include <cstdlib>
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

int main()
{
    // sum [0, 5)
    auto const sum = accumulate(make_range<int>(0, 5), 0);
    printf("sum: %d\n", sum);

    return EXIT_SUCCESS;
}