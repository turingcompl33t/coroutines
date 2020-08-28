// driver.cpp

#include <vector>
#include <cassert>
#include <cstdlib>

#include "map.hpp"
#include "multilookup.hpp"

constexpr static auto const N_INSERT = 128ul;
constexpr static auto const N_LOOKUP = 64ul;

static void test_sequential_multilookup()
{
    Map<int, int> map{};
    
    for (auto i = 0ul; i < N_INSERT; ++i)
    {
        auto const as_int = static_cast<int>(i);
        map.insert(as_int, as_int);
    }

    assert(map.count() == N_INSERT);

    std::vector<int> lookups{};
    lookups.reserve(N_LOOKUP);
    for (auto i = 0ul; i < N_LOOKUP; ++i)
    {
        lookups.push_back(static_cast<int>(i));
    }

    auto results = sequential_multilookup(map, lookups);
    
    assert(results.size() == N_LOOKUP);
}

static void test_interleaved_multilookup()
{
    Map<int, int> map{};
    
    for (auto i = 0ul; i < N_INSERT; ++i)
    {
        auto const as_int = static_cast<int>(i);
        map.insert(as_int, as_int);
    }

    assert(map.count() == N_INSERT);

    std::vector<int> lookups{};
    lookups.reserve(N_LOOKUP);
    for (auto i = 0ul; i < N_LOOKUP; ++i)
    {
        lookups.push_back(static_cast<int>(i));
    }

    auto results = interleaved_multilookup(map, lookups, 4);
    
    assert(results.size() == N_LOOKUP);
}

int main()
{
    test_sequential_multilookup();
    test_interleaved_multilookup();
    return EXIT_SUCCESS;
}