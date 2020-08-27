// map.hpp
// A simple chaining hashmap implementation with coroutine support for bulk lookups.

#ifndef MAP_HPP
#define MAP_HPP

#include <cstdlib>
#include <functional>
#include <stdcoro/coroutine.hpp>

// ----------------------------------------------------------------------------
// Interface

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher = std::hash<KeyT>>
class Map
{
    // the initial number of buckets allocated to internal table
    constexpr static auto const INIT_CAPACITY = 8ul;

    // the target load factor for the map
    constexpr static auto const TARGET_LOAD_FACTOR = 0.5;

    struct Entry;
    struct Bucket;

    class  LookupKVResult;
    class  InsertKVResult;

    // the current number of items in the table
    std::size_t n_items;

    // the current number of buckets in the table
    std::size_t capacity;
    
    // the array of buckets that composes the table
    Bucket* buckets;

    // the hash functor used to hash keys
    Hasher hasher;

public:
    Map();

    ~Map();
    
    // non-copyable
    Map(Map const&)            = delete;
    Map& operator=(Map const&) = delete;

    // non-movable
    Map(Map&&)            = delete;
    Map& operator=(Map&&) = delete;

    // lookup an item in the map by key 
    auto lookup(KeyT const& key) -> LookupKVResult;

    // insert a new key / value pair into the map
    auto insert(KeyT const& key, ValueT value) -> InsertKVResult;

    // query the current number of items in the map
    auto count() const -> std::size_t;

private:
    auto bucket_index_for_key(KeyT const& key) -> std::size_t;

    auto resize_required() const -> bool;
    auto perform_resize() -> void;

    auto insert_into_bucket(Bucket& bucket, Entry* entry) -> void;
};

// ----------------------------------------------------------------------------
// Auxiliary Types

template <typename KeyT, typename ValueT, typename Hasher>
struct Map<KeyT, ValueT, Hasher>::Entry
{
    Entry* next;

    KeyT   key;
    ValueT value;

    Entry() 
        : next{nullptr}, key{}, value{} {}

    Entry(KeyT const& key_, ValueT& value_)
        : next{nullptr}, key{key_}, value{value_} {}
};

template <typename KeyT, typename ValueT, typename Hasher>
struct Map<KeyT, ValueT, Hasher>::Bucket
{   
    // pointer to the first entry in the bucket chain, if any
    Entry* first;

    // the number of items in this bucket
    std::size_t n_items;

    Bucket() 
        : first{nullptr}, n_items{0} {}
};

// type returned by a lookup operation
template <typename KeyT, typename ValueT, typename Hasher>
class Map<KeyT, ValueT, Hasher>::LookupKVResult
{
    KeyT const* key;
    ValueT*     value;

public:
    LookupKVResult() 
        : key{nullptr}
        , value{nullptr} {}

    LookupKVResult(KeyT const& key_, ValueT& value_)
        : key{&key_}, value{&value_} {}

    KeyT const& get_key() const
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
    KeyT const* key;
    ValueT*     value;
    bool const  inserted;

public:
    InsertKVResult(KeyT const& key_, ValueT& value_, bool const inserted_)
        : key{&key_}, value{&value_}, inserted{inserted_} {}

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

// ----------------------------------------------------------------------------
// Exported Definitions

template <typename KeyT, typename ValueT, typename Hasher>
Map<KeyT, ValueT, Hasher>::Map()
    : n_items{0}
    , capacity{INIT_CAPACITY}
    , buckets{new Bucket[INIT_CAPACITY]}
    , hasher{}
{}

template <typename KeyT, typename ValueT, typename Hasher>
Map<KeyT, ValueT, Hasher>::~Map()
{
    delete[] buckets;
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::lookup(KeyT const& key) -> LookupKVResult
{
    auto const index = bucket_index_for_key(key);

    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // not found
        return LookupKVResult{};
    }

    auto& entry = *bucket.first;
    for (;;)
    {
        if (key == entry.key)
        {
            return LookupKVResult{entry.key, entry.value};
        }

        if (nullptr == entry.next)
        {
            // reached the end of the bucket chain
            break;
        }

        // traverse the linked-list of entries for this bucket
        entry = *(entry.next);
    }

    // not found
    return LookupKVResult{};
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::insert(KeyT const& key, ValueT value) -> InsertKVResult
{
    if (resize_required())
    {
        perform_resize();
    }

    auto const index = bucket_index_for_key(key);
    
    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // empty bucket chain
        Entry* new_entry = new Entry{key, value};
        bucket.first = new_entry;
        ++bucket.n_items;
        ++n_items;

        return InsertKVResult{key, value, true};
    }

    auto& entry = *bucket.first;
    for (;;)
    {
        if (key == entry.key)
        {
            // collision; do not insert
            return InsertKVResult{entry.key, entry.value, false};
        }

        if (nullptr == entry.next)
        {
            // reached the end of the entry chain
            break;
        }

        entry = *entry.next;
    }

    // reached the end of the entry chain without collision;
    // insert this key / value pair at end of current chain

    auto* new_entry = new Entry{key, value};
    entry.next = new_entry;
    ++bucket.n_items;
    ++n_items;

    return InsertKVResult{key, value, true};
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::count() const -> std::size_t
{
    return n_items;
}

// ----------------------------------------------------------------------------
// Internal Definitions

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::bucket_index_for_key(KeyT const& key) -> std::size_t
{
    auto const hash = hasher(key);
    // NOTE: we rely on the fact that the number of buckets
    // in the internal table is always a power of 2 here
    return (hash & (capacity - 1));
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::resize_required() const -> bool
{
    return (static_cast<double>(n_items) 
        / static_cast<double>(capacity)) > TARGET_LOAD_FACTOR; 
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::perform_resize() -> void
{   
    // double the size of the table on resize
    auto const new_capacity = (capacity << 1);
    auto* new_buckets = new Bucket[new_capacity];

    for (auto i = 0ul; i < capacity; ++i)
    {
        // iterate over each bucket in the current table
        auto& bucket = buckets[i];
        
        auto* entry = bucket.first;
        while (entry != nullptr)
        {
            auto const new_index = (hasher(entry->key) & (new_capacity - 1));
            auto& new_bucket = new_buckets[new_index];

            insert_into_bucket(new_bucket, entry);

            entry = entry->next;
        }
    }

    delete[] buckets;

    buckets  = new_buckets;
    capacity = new_capacity;
}

template <typename KeyT, typename ValueT, typename Hasher>
auto Map<KeyT, ValueT, Hasher>::insert_into_bucket(Bucket& bucket, Entry* entry) -> void
{
    auto* current = bucket.first;
    if (nullptr == current)
    {
        bucket.first = entry;
        return;
    }

    // advance to the end of the bucket chain
    for (; current->next != nullptr; current = current->next);
    current->next = entry;

    ++bucket.n_items;
}

#endif // MAP_HPP