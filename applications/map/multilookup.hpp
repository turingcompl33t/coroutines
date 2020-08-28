// multilookup.hpp

#ifndef MULTILOOKUP_HPP
#define MULTILOOKUP_HPP

#include "map.hpp"

// use Map::lookup_sync() to sequentially perform a lookup for each key in `keys`
template <typename KeyT, typename ValueT, typename Hasher>
auto sequential_multilookup(
    Map<KeyT, ValueT, Hasher>& map, 
    std::vector<KeyT>          keys) -> std::vector<typename Map<KeyT, ValueT, Hasher>::LookupKVResult>
{
    std::vector<typename Map<KeyT, ValueT, Hasher>::LookupKVResult> results{};
    results.reserve(keys.size());

    for (auto const& key : keys)
    {
        results.push_back(map.sync_lookup(key));
    }

    return results;
}

// use Map::lookup_async() to multiplex many coroutines for instruction stream interleaving
template <typename KeyT, typename ValueT, typename Hasher>
auto interleaved_multilookup(
    Map<KeyT, ValueT, Hasher>& map, 
    std::vector<KeyT>          keys,
    std::size_t const          n_streams) -> std::vector<typename Map<KeyT, ValueT, Hasher>::LookupKVResult>
{
    using ResultType = typename Map<KeyT, ValueT, Hasher>::LookupKVResult;

    Throttler<KeyT, ValueT, Hasher> throttler{n_streams};

    std::vector<ResultType> results{};
    results.reserve(keys.size());

    for (auto const& key : keys)
    {
        throttler.spawn(
            map.async_lookup(
                key,
                [&results](KeyT const& k, ValueT& v){ results.push_back(ResultType{k, v}); },  // on_found()
                [&results](){ results.push_back(ResultType{}); } ));                           // on_not_found()
    }

    // run until all lookup tasks complete
    throttler.run();

    return results;
}

#endif // MULTILOOKUP_HPP