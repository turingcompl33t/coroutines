// stats.cpp
// Driver program to collect map statistics for various insertion counts.

#include <cstdlib>
#include <iostream>

#include "map.hpp"

// the maximum number of buckets allocated by the map
constexpr static std::size_t const MAP_MAX_CAPACITY = 1 << 16;

// upper and lower bound for number of insertions to perform
constexpr static std::size_t const MIN_N_ITEMS = 1 << 16;  // ~65,000 keys
constexpr static std::size_t const MAX_N_ITEMS = 1 << 25;  // ~33 million keys

static void perform_inserts_and_dump_stats(std::size_t const n_items)
{
    Map<int, int> map{MAP_MAX_CAPACITY};

    for (auto i = 0ul; i < n_items; ++i)
    {
        auto const kv = static_cast<int>(i);
        map.insert(kv, kv);
    }

    auto stats = map.stats();

    // the total number of bytes consumed by items in the map;
    // each entry holds a key / value pair, along with a `next` pointer
    auto const total_item_bytes 
        = stats.count * ((sizeof(int)*2) + sizeof(void*));
    auto const as_mb = total_item_bytes / static_cast<std::size_t>(1 << 20);

    std::cout << "[+] map statistics:\n"
        << "\titem count:           " << stats.count << '\n'
        << "\tcapacity:             " << stats.capacity << '\n'
        << "\tmax capacity:         " << stats.max_capacity << '\n'
        << "\tload factor:          " << stats.load_factor << '\n'
        << "\tmin bucket depth:     " << stats.min_bucket_depth << '\n'
        << "\tmax bucket depth:     " << stats.max_bucket_depth << '\n'
        << "\tavg bucket depth:     " << stats.avg_bucket_depth << '\n'
        << "\ttotal item footprint: " << as_mb << " MB\n"
        << std::flush;
}

int main()
{
    for (auto i = MIN_N_ITEMS; i <= MAX_N_ITEMS; i <<= 1)
    {
        perform_inserts_and_dump_stats(i);
    }

    return EXIT_SUCCESS;
}