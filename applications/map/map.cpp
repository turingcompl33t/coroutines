// map.cpp
// A simple chaining hashmap implementation with coroutine support for bulk lookups.

#include "map.hpp"

template <typename KeyT, typename ValueT, typename Hasher>
class Map<KeyT, ValueT, Hasher>::Entry
{
    Entry* next;

    KeyT   key;
    ValueT value;

public:
    Entry() 
        : next{nullptr}
        , key{}
        , value{} {}
};

// type returned by a lookup operation
template <typename KeyT, typename ValueT, typename Hasher>
class Map<KeyT, ValueT, Hasher>::LookupKVResult
{
    KeyT*   key;
    ValueT* value;

public:
    LookupKVResult() 
        : key{nullptr}
        , value{nullptr} {}

    KeyT& get_key() const
    {
        return *key;
    }

    ValueT& get_value() const
    {
        return *value;
    }

    explicit operator bool() const
    {
        return (key != nullptr);
    }
};

// type returned by an insert operation
template <typename KeyT, typename ValueT, typename Hasher>
class Map<KeyT, ValueT, Hasher>::InsertKVResult
{
    bool    inserted;
    KeyT*   key;
    ValueT* value;

public:
    bool successful() const
    {
        return inserted;
    }

    KeyT& get_key() const
    {
        return *key;
    }

    ValueT& get_value() const
    {
        return *value;
    }
};

template <typename KeyT, typename ValueT, typename Hasher>
Map<KeyT, ValueT, Hasher>::Map()
    : n_items{0}
    , n_buckets{INIT_N_BUCKETS}
    , buckets{new Entry[INIT_N_BUCKETS]}
{}

template <typename KeyT, typename ValueT, typename Hasher>
Map<KeyT, ValueT, Hasher>::~Map()
{
    delete[] buckets;
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::lookup(KeyT const& key) -> LookupKVResult
{

}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::insert(KeyT const& key, ValueT value) -> InsertKVResult
{

}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::count() const -> std::size_t
{
    return n_items;
}