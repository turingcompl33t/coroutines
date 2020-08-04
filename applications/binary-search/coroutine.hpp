// coroutine.hpp
// Interleaved binary search via coroutines.
//
// Adapted from original code by Gor Nishanov.

#ifndef COROUTINE_BS_H
#define COROUTINE_BS_H

#include <vector>
#include <cstdio>
#include <cassert>

#include "coro_infra.hpp"

// hacky way to do this...
static size_t found_count     = 0;
static size_t not_found_count = 0;

template <typename Iter, typename Found, typename NotFound>
root_task coro_binary_search(
    Iter      first, 
    Iter      last, 
    int const key, 
    Found     on_found, 
    NotFound  on_not_found)
{
    auto len = last - first;
    while (len > 0)
    {
        auto half = len / 2;
        auto middle = first + half;
        
        auto x = co_await prefetch(*middle);
        
        if (x < key)
        {
            first = middle;
            ++first;
            len = len - half - 1;
        }
        else
        {
            len = half;
        }

        if (x == key)
        {
            co_return on_found();
        }
    }

    on_not_found();
}

static void on_found()
{
    found_count++;
}

static void on_not_found()
{
    not_found_count++;
}

long coro_multi_lookup(
    std::vector<int> const& dataset,
    std::vector<int> const& lookups,
    size_t n_streams)
{
    throttler t{n_streams};

    for (auto key : lookups)
    {
        t.spawn(
            coro_binary_search(
                dataset.begin(), 
                dataset.end(), 
                key, 
                on_found, 
                on_not_found));
    }

    t.run();

    assert((found_count + not_found_count) == lookups.size());

    auto const tmp = found_count;

    // reset counters for next iteration (if applicable)
    found_count     = 0;
    not_found_count = 0;

    return tmp;
}

#endif // COROUTINE_BS_H