// driver.cpp

#include <vector>
#include <cassert>
#include <cstdlib>

#include "map.hpp"

constexpr static auto const N_INSERT = 128ul;
constexpr static auto const N_LOOKUP = 64ul;

static std::vector<int> make_lookups(std::size_t const n)
{
    std::vector<int> lookups{};
    lookups.reserve(n);
    for (auto i = 0ul; i < N_LOOKUP; ++i)
    {
        lookups.push_back(static_cast<int>(i));
    }

    return lookups;
}

static void drive_sequential_multilookup()
{
    Map<int, int> map{};
    
    for (auto i = 0ul; i < N_INSERT; ++i)
    {
        auto const as_int = static_cast<int>(i);
        auto r = map.insert(as_int, as_int);
        
        assert(static_cast<bool>(r));
    }

    assert(map.count() == N_INSERT);

    auto lookups = make_lookups(N_LOOKUP);
    auto results = map.sequential_multilookup(lookups);
    
    assert(results.size() == N_LOOKUP);

    for (auto& lookup_result : results)
    {
        assert(static_cast<bool>(lookup_result));

        auto const& k = lookup_result.get_key();
        auto const& v = lookup_result.get_value();
        assert(k == v);
    }
}

// static void test_interleaved_multilookup()
// {
//     Map<int, int> map{};
    
//     for (auto i = 0ul; i < N_INSERT; ++i)
//     {
//         auto const as_int = static_cast<int>(i);
//         map.insert(as_int, as_int);
//     }

//     assert(map.count() == N_INSERT);

//     std::vector<int> lookups{};
//     lookups.reserve(N_LOOKUP);
//     for (auto i = 0ul; i < N_LOOKUP; ++i)
//     {
//         lookups.push_back(static_cast<int>(i));
//     }

//     auto results = interleaved_multilookup(map, lookups, 4);
    
//     assert(results.size() == N_LOOKUP);
// }

int main()
{
    drive_sequential_multilookup();
    //test_interleaved_multilookup();
    return EXIT_SUCCESS;
}