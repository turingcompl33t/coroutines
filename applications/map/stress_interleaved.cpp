// stress_interleaved.cpp

#include <vector>
#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdcoro/coroutine.hpp>
#include <libcoro/generator.hpp>

#include "map.hpp"
#include "scheduler.hpp"

constexpr static std::size_t const MAP_MAX_CAPACITY = 1 << 16;

static coro::generator<int> 
make_lookup_range(std::size_t const n)
{
    for (auto i = 0ul; i < n; ++i)
    {
        co_yield static_cast<int>(i);
    }
}

static void stress_interleaved_multilookup(
    std::size_t const n_inserts, 
    std::size_t const n_lookups)
{
    using ResultType = typename Map<int, int>::LookupResultType;

    // map with explicit maximum capacity
    Map<int, int> map{MAP_MAX_CAPACITY};

    // task scheduler for multilookup
    StaticQueueScheduler<32> scheduler{};

    std::cout << "[+] inserting " << n_inserts << " key value pairs...\n";
    
    for (auto i = 0ul; i < n_inserts; ++i)
    {
        auto const as_int = static_cast<int>(i);
        map.insert(as_int, as_int);
    }

    assert(map.count() == n_inserts);

    std::cout << "[+] performing " << n_lookups << " lookups for inserted keys...\n";

    // prepare an output vector for results
    std::vector<ResultType> results{};
    results.reserve(n_lookups);

    auto lookups = make_lookup_range(n_lookups);
    map.interleaved_multilookup(
        lookups.begin(), 
        lookups.end(), 
        std::back_inserter(results), 
        scheduler, 4); // 4 streams
    
    assert(results.size() == n_lookups);

    for (auto& lookup_result : results)
    {
        assert(static_cast<bool>(lookup_result));

        auto const& k = lookup_result.get_key();
        auto const& v = lookup_result.get_value();
        assert(k == v);
    }

    auto const stats = map.stats();

    std::cout << "[+] map statistics:\n"
        << "\titem count:       " << stats.count << '\n'
        << "\tcapacity:         " << stats.capacity << '\n'
        << "\tmax capacity:     " << stats.max_capacity << '\n'
        << "\tload factor:      " << stats.load_factor << '\n'
        << "\tmin bucket depth: " << stats.min_bucket_depth << '\n'
        << "\tmax bucket depth: " << stats.max_bucket_depth << '\n'
        << "\tavg bucket depth: " << stats.avg_bucket_depth << '\n'
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