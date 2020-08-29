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

template <
    typename BeginIter, 
    typename EndIter, 
    typename Accumulator>
Accumulator accumulate_iter(
    BeginIter   begin, 
    EndIter     end, 
    Accumulator init)
{
    Accumulator accumulator{init};
    for (auto iter = begin; iter != end; ++iter)
    {
        accumulator += *iter;
    }

    return accumulator;
}

template <typename RangeType, typename Accumulator>
Accumulator accumulate_range(RangeType&& range, Accumulator init)
{
    Accumulator accumulator{init};
    for (auto value : range)
    {
        accumulator += value;
    }

    return accumulator;
}

int main()
{
    auto range = make_range<int>(0, 5);
    auto const sum_iter = accumulate_iter(range.begin(), range.end(), 0);

    auto const sum_range = accumulate_range(make_range<int>(0, 5), 0);

    printf("sum iter:  %d\n", sum_iter);
    printf("sum range: %d\n", sum_range);

    return EXIT_SUCCESS;
}