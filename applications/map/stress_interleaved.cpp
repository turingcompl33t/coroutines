// stress_interleaved.cpp

#include <vector>
#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "map.hpp"
#include "scheduler.hpp"

static std::vector<int> make_lookups(std::size_t const n)
{
    std::vector<int> lookups{};
    lookups.reserve(n);
    for (auto i = 0ul; i < n; ++i)
    {
        lookups.push_back(static_cast<int>(i));
    }

    return lookups;
}

static void stress_interleaved_multilookup(
    std::size_t const n_inserts, 
    std::size_t const n_lookups)
{
    Map<int, int> map{};

    StaticQueueScheduler<32> scheduler{};

    std::cout << "[+] inserting " << n_inserts << " key value pairs...\n";
    
    for (auto i = 0ul; i < n_inserts; ++i)
    {
        auto const as_int = static_cast<int>(i);
        map.insert(as_int, as_int);
    }

    assert(map.count() == n_inserts);

    std::cout << "[+] performing " << n_lookups << " lookups for inserted keys...\n";

    auto lookups = make_lookups(n_lookups);
    auto results = map.interleaved_multilookup(lookups, scheduler, 4); // 4 streams
    
    assert(results.size() == n_lookups);

    auto const stats = map.stats();

    std::cout << "[+] map statistics:\n"
        << "\titem count:       " << stats.count << '\n'
        << "\tcapacity:         " << stats.capacity << '\n'
        << "\tload factor:      " << stats.load_factor << '\n'
        << "\tmin bucket chain: " << stats.min_bucket_chain_length << '\n'
        << "\tmax bucket chain: " << stats.max_bucket_chain_length << '\n'
        << "\tavg bucket chain: " << stats.avg_bucket_chain_length << '\n'
        << std::flush;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "[-] invalid arguments\n"
            << "[-] usage: " << argv[0] << " <N_INSERTIONS> <N_LOOKUPS>\n"
            << std::flush;
        return EXIT_FAILURE;
    }

    // not going to try to recover if these throw
    auto const n_inserts = std::stoul(argv[1]); 
    auto const n_lookups = std::stoul(argv[2]);

    stress_interleaved_multilookup(n_inserts, n_lookups);

    return EXIT_SUCCESS;
}