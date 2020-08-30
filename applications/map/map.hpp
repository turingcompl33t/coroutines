// map.hpp
// A simple chaining hashmap implementation with coroutine support for bulk lookups.
//
// The API for this map is inspired by that described by Chandler Carruth
// in his C++Now 2019 Lightning Talk "A clean and minimal map API":
// https://www.youtube.com/watch?v=kye4aD-KvTU

#ifndef MAP_HPP
#define MAP_HPP

#include <vector>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <functional>
#include <stdcoro/coroutine.hpp>

#include "prefetch.hpp"
#include "throttler.hpp"
#include "recycling_allocator.hpp"

// ----------------------------------------------------------------------------
// Misc. Helper Declarations

static std::size_t next_power_of_2(std::size_t n);

// ----------------------------------------------------------------------------
// Interface

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher = std::hash<KeyT>>
class Map
{
    // The default initial number of buckets allocated to internal table.
    // If a user-provided initial capacity exceeds this value, this value is used.
    constexpr static auto const DEFAULT_INIT_CAPACITY = 16ul;

    // The default maximum capacity for the map.
    constexpr static auto const DEFAULT_MAX_CAPACITY = std::numeric_limits<std::size_t>::max();

    // The maximum load factor; once exceeded, a resize operation is 
    // triggered, unless the maximum capacity has already been reached.
    constexpr static auto const MAX_LOAD_FACTOR = 0.5;

    struct Entry;
    struct Bucket;

    template <typename Scheduler>
    class LookupKVTask;

    // The current number of items in the map.
    std::size_t n_items;

    // The current number of buckets in the table.
    std::size_t capacity;

    // The maximum number of buckets beyond which further
    // inserations will no longer trigger a resize.
    std::size_t const max_capacity;
    
    // the array of buckets that composes the table
    Bucket* buckets;

    // the hash functor used to hash keys
    Hasher hasher;

public:
    class LookupKVResult;
    class InsertKVResult;
    class RemoveKVResult;

    struct StatsResult;

    using LookupResultType = LookupKVResult;
    using InsertResultType = InsertKVResult;
    using UpdateResultType = InsertKVResult;
    using RemoveResultType = RemoveKVResult;

    Map();

    explicit Map(std::size_t const max_capacity_);

    ~Map();
    
    // non-copyable
    Map(Map const&)            = delete;
    Map& operator=(Map const&) = delete;

    // non-movable
    Map(Map&&)            = delete;
    Map& operator=(Map&&) = delete;

    // Lookup an item in the map by key.
    auto lookup(KeyT const& key) -> LookupKVResult;

    // Insert a new key / value pair into the map;
    // does not insert if key is already present.
    auto insert(KeyT const& key, ValueT value) -> InsertKVResult;

    // Update the value associated with `key` in the map;
    // if `key` is not present, key / value pair is inserted.
    auto update(KeyT const& key, ValueT value) -> InsertKVResult;

    // Remove a key / value pair from the map.
    auto remove(KeyT const& key) -> RemoveKVResult;

    // Perform a lookup operation for each key in the
    // range [begin_keys, end_keys), inserting the result
    // of each lookup operation into the range `begin_results`.
    // Each lookup operation is performed sequentially.
    template <
        typename BeginInputIter, 
        typename EndInputIter, 
        typename OutputIter>
    auto sequential_multilookup(
        BeginInputIter begin_keys,
        EndInputIter   end_keys,
        OutputIter     begin_results) -> void;

    // Perform a lookup operation for each key in the
    // range [begin_keys, end_keys), inserting the result
    // of each lookup operation into the range `begin_results`.
    // Lookup operations are spawned as independent coroutines
    // such that their instruction streams may be interleaved
    // in order to hide memory stall latency for large maps.
    template <
        typename BeginInputIter, 
        typename EndInputIter, 
        typename OutputIter,
        typename Scheduler>
    auto interleaved_multilookup(
        BeginInputIter    begin_keys,
        EndInputIter      end_keys,
        OutputIter        begin_results,
        Scheduler const&  scheduler,
        std::size_t const n_streams) -> void;

    // Query the current number of items in the map.
    auto count() const -> std::size_t;

    // Compute some instance-specific statistics and return to the caller.
    auto stats() const -> StatsResult;

private:
    template <
        typename Scheduler, 
        typename OnFound, 
        typename OnNotFound>
    auto lookup_task(
        KeyT const       key, 
        Scheduler const& scheduler,
        OnFound          on_found, 
        OnNotFound       on_not_found) -> LookupKVTask<Scheduler>;

    auto bucket_index_for_key(KeyT const& key) -> std::size_t;

    auto perform_resize_if_required() -> void;
    auto resize_required() const -> bool;

    auto insert_into_bucket(Bucket& bucket, Entry* entry) -> void;
};

// ----------------------------------------------------------------------------
// Auxiliary Types (Internal)

// An individual entry in the internal hashtable.
//
// The layout of this structure is important as it
// determines the per-item overhead for the map.
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

// The head of a bucket chain in the internal hashtable.
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

// ----------------------------------------------------------------------------
// Auxiliary Types (Internal, Coroutine-Specific)

// The task type used to represent interleaved 
// lookup operations in Map::interleaved_multilookup(). 
template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
template <typename Scheduler>
class Map<KeyT, ValueT, Hasher>::LookupKVTask
{
public:
    struct promise_type;
    using CoroHandle = stdcoro::coroutine_handle<promise_type>;

    LookupKVTask() = delete;

    // IMPT: not an RAII type; the throttler instance takes 
    // ownership of the LookupKVTask immediately upon spawning it.
    ~LookupKVTask() = default;

    LookupKVTask(LookupKVTask const&) = delete;

    LookupKVTask(LookupKVTask&& rhs) 
        : handle{rhs.handle}
    {
        rhs.handle = nullptr;
    }

    struct promise_type
    {
        // pointer to the owning throttler instance for this promise
        Throttler<Scheduler>* owning_throttler{nullptr};

        // utilize the custom recycling allocator
        void* operator new(std::size_t n)
        {
            return inline_recycling_allocator.alloc(n);
        }

        // utilize the custom recycling allocator
        void operator delete(void* ptr, std::size_t n)
        {
            inline_recycling_allocator.free(ptr, n);
        }

        LookupKVTask get_return_object()
        {
            return LookupKVTask{*this};
        }

        auto initial_suspend()
        {
            return std::suspend_always{};
        }

        auto final_suspend()
        {
            return std::suspend_never{};
        }

        void return_void()
        {
            owning_throttler->on_task_complete();
        }

        void unhandled_exception() noexcept
        {
            std::terminate();
        }
    };

    // make the throttler the "owner" of this coroutine
    auto set_owner(Throttler<Scheduler>* owner)
    {
        auto result = handle;
        
        // modify the promise to point to the owning throttler
        handle.promise().owning_throttler = owner;
        
        // reset our handle
        handle = nullptr;

        return result;
    }

private:
    CoroHandle handle;

    LookupKVTask(promise_type& p)
        : handle{CoroHandle::from_promise(p)} {}
};

// ----------------------------------------------------------------------------
// Auxiliary Types (Exported)

// The type returned by Map::lookup() operations.
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

// The type returned by Map::insert() and Map::update() operations.
template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
class Map<KeyT, ValueT, Hasher>::InsertKVResult
{
    KeyT const* key;
    ValueT*     value;
    bool const  inserted;

public:
    InsertKVResult(
        KeyT const& key_, 
        ValueT&     value_, 
        bool const  inserted_)
        : key{&key_}
        , value{&value_}
        , inserted{inserted_} {}

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
        return inserted;
    }
};

// The type returned by Map::remove() operations.
template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
class Map<KeyT, ValueT, Hasher>::RemoveKVResult
{
    std::optional<KeyT>   key;
    std::optional<ValueT> value;

public:
    RemoveKVResult()
        : key{}, value{} {}

    RemoveKVResult(KeyT&& key_, ValueT&& value_)
        : key{std::move(key_)}, value{std::move(value_)} {}

    KeyT& get_key()
    {
        return *key;
    }

    ValueT& get_value()
    {
        return *value;
    }

    KeyT take_key()
    {
        return *std::move(key);
    }

    ValueT take_value()
    {
        return *std::move(value);
    }

    explicit operator bool()
    {
        return static_cast<bool>(key);
    }
};

// The type returned by Map::stats() operations.
template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
struct Map<KeyT, ValueT, Hasher>::StatsResult
{
    // The current count of items in the map.
    std::size_t count;

    // The current capacity of the map (number of buckets).
    std::size_t capacity;

    // The maximum capacity of the map (number of buckets).
    std::size_t max_capacity; 

    // The current map load factor.
    double  load_factor;

    // The minimum length of a bucket chain in the map.
    std::size_t min_bucket_depth;

    // The maxumum length of a bucket chain in the map.
    std::size_t max_bucket_depth;

    // The average length of a bucket chain in the map.
    std::size_t avg_bucket_depth;
};

// ----------------------------------------------------------------------------
// Exported Definitions

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
Map<KeyT, ValueT, Hasher>::Map() 
    : Map{DEFAULT_MAX_CAPACITY} {}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
Map<KeyT, ValueT, Hasher>::Map(
    std::size_t const max_capacity_)
    : n_items{0}
    , capacity{0}
    , max_capacity{max_capacity_}
    , buckets{nullptr}
    , hasher{}
{
    if (0 == max_capacity)
    {
        throw std::runtime_error{"maximum capacity must be nonzero"};
    }

    // ensure the initial capacity is a power of 2
    auto const init_capacity = 
        next_power_of_2(std::min(DEFAULT_INIT_CAPACITY, max_capacity)) >> 1;

    capacity = init_capacity;
    buckets  = new Bucket[init_capacity];
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
Map<KeyT, ValueT, Hasher>::~Map()
{
    delete[] buckets;
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::lookup(KeyT const& key) -> LookupKVResult
{
    auto const index = bucket_index_for_key(key);

    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // not found
        return LookupKVResult{};
    }

    auto* entry = bucket.first;
    for (;;)
    {
        if (key == entry->key)
        {
            return LookupKVResult{entry->key, entry->value};
        }

        if (nullptr == entry->next)
        {
            // reached the end of the bucket chain
            break;
        }

        // traverse the linked-list of entries for this bucket
        entry = entry->next;
    }

    // not found
    return LookupKVResult{};
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::insert(
    KeyT const& key, 
    ValueT      value) -> InsertKVResult
{
    auto const index = bucket_index_for_key(key);
    
    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // empty bucket chain
        auto* new_entry = new Entry{key, value};
        bucket.first = new_entry;
        ++bucket.n_items;
        ++n_items;

        perform_resize_if_required();

        return InsertKVResult{key, value, true};
    }

    auto* entry = bucket.first;
    for (;;)
    {
        if (key == entry->key)
        {
            // collision; do not insert
            return InsertKVResult{entry->key, entry->value, false};
        }

        if (nullptr == entry->next)
        {
            // reached the end of the bucket chain
            break;
        }

        entry = entry->next;
    }

    // reached the end of the entry chain without collision;
    // insert this key / value pair at end of current chain

    auto* new_entry = new Entry{key, value};
    entry->next = new_entry;
    ++bucket.n_items;
    ++n_items;

    perform_resize_if_required();

    return InsertKVResult{key, value, true};
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::update(
    KeyT const& key, 
    ValueT      value) -> InsertKVResult
{
    auto const index = bucket_index_for_key(key);
    
    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // empty bucket chain
        auto* new_entry = new Entry{key, value};
        bucket.first = new_entry;
        ++bucket.n_items;
        ++n_items;

        perform_resize_if_required();

        return InsertKVResult{key, value, true};
    }

    auto* entry = bucket.first;
    for (;;)
    {
        if (key == entry->key)
        {
            // found a matching key; update the associated value
            entry->value.~ValueT();
            entry->value = value;
            return InsertKVResult{entry->key, entry->value, true};
        }

        if (nullptr == entry->next)
        {
            // reached the end of the bucket chain
            break;
        }

        entry = entry->next;
    }

    // reached the end of the entry chain without collision;
    // insert this key / value pair at end of current chain

    auto* new_entry = new Entry{key, value};
    entry->next = new_entry;
    ++bucket.n_items;
    ++n_items;

    perform_resize_if_required();

    return InsertKVResult{key, value, true};
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::remove(
    KeyT const& key) -> RemoveKVResult
{
    auto const index = bucket_index_for_key(key);

    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // key not present in the map
        return RemoveKVResult{};
    }

    Entry* prev = nullptr;
    auto* entry = bucket.first;
    for (;;)
    {
        if (key == entry->key)
        {
            // found a matching key; remove the key value pair
            auto result = RemoveKVResult{std::move(entry->key), std::move(entry->value)};
            if (prev != nullptr)
            {
                prev->next = entry->next;
            }
            else
            {
                bucket.first = entry->next;
            }
            
            delete entry;
            --bucket.n_items;
            --n_items;

            return result;
        }

        if (nullptr == entry->next)
        {
            // reached the end of the bucket chain
            break;
        }

        entry = entry->next;
    }
    
    // key not present
    return RemoveKVResult{};
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
template <
    typename BeginInputIter, 
    typename EndInputIter, 
    typename OutputIter>
auto Map<KeyT, ValueT, Hasher>::sequential_multilookup(
    BeginInputIter begin_keys,
    EndInputIter   end_keys,
    OutputIter     begin_results) -> void
{
    for (auto iter = begin_keys; iter != end_keys; ++iter)
    {
        *begin_results = lookup(*iter);
        ++begin_results;
    }
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
template <
    typename BeginInputIter, 
    typename EndInputIter, 
    typename OutputIter,
    typename Scheduler>
auto Map<KeyT, ValueT, Hasher>::interleaved_multilookup(
    BeginInputIter    begin_keys,
    EndInputIter      end_keys,
    OutputIter        begin_results,
    Scheduler const&  scheduler,
    std::size_t const n_streams) -> void
{
    using MapType    = Map<KeyT, ValueT, Hasher>;
    using ResultType = typename MapType::LookupKVResult;

    // instantiate a throttler for this multilookup
    Throttler throttler{scheduler, n_streams};

    for (auto key_iter = begin_keys; key_iter != end_keys; ++key_iter)
    {
        throttler.spawn(
            lookup_task(
                *key_iter,
                scheduler,
                [&begin_results](KeyT const& k, ValueT& v) mutable {
                    *begin_results = ResultType{k, v};
                    ++begin_results;
                },
                [&begin_results]() mutable { 
                    *begin_results = ResultType{};
                    ++begin_results;
                }));
    }

    // run until all lookup tasks complete
    throttler.run();
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::count() const -> std::size_t
{
    return n_items;
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::stats() const -> StatsResult
{
    StatsResult results{};

    results.count        = n_items;
    results.capacity     = capacity;
    results.max_capacity = max_capacity;

    results.load_factor 
        = static_cast<double>(n_items) / static_cast<double>(capacity);

    std::size_t min_length = std::numeric_limits<std::size_t>::max();
    std::size_t max_length = 0;
    std::size_t sum_length = 0;
    for (auto i = 0ul; i < capacity; ++i)
    {   
        auto& bucket = buckets[i];
        min_length = std::min(bucket.n_items, min_length);
        max_length = std::max(bucket.n_items, max_length);
        sum_length += bucket.n_items;
    }

    results.min_bucket_depth = min_length;
    results.max_bucket_depth = max_length;
    results.avg_bucket_depth = sum_length / capacity;

    return results;
}

// ----------------------------------------------------------------------------
// Internal Definitions

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
template < 
    typename Scheduler, 
    typename OnFound, 
    typename OnNotFound>
auto Map<KeyT, ValueT, Hasher>::lookup_task(
    KeyT const       key, 
    Scheduler const& scheduler,
    OnFound          on_found, 
    OnNotFound       on_not_found) -> LookupKVTask<Scheduler>
{
    auto const index = bucket_index_for_key(key);

    // assume that the bucket array itself is cached
    // TODO: can we avoid this assumption?
    auto& bucket = buckets[index];
    if (0 == bucket.n_items)
    {
        // not found
        co_return on_not_found();
    }

    auto* entry = co_await prefetch_and_schedule_on(bucket.first, scheduler);
    for (;;)
    {
        if (key == entry->key)
        {
            co_return on_found(entry->key, entry->value);
        }

        if (nullptr == entry->next)
        {
            // reached the end of the bucket chain
            break;
        }

        // traverse the linked-list of entries for this bucket
        entry = co_await prefetch_and_schedule_on(entry->next, scheduler);
    }

    // not found
    co_return on_not_found();
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::bucket_index_for_key(KeyT const& key) -> std::size_t
{
    auto const hash = hasher(key);
    // NOTE: we rely on the fact that the number of buckets
    // in the internal table is always a power of 2 here
    return (hash & (capacity - 1));
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::perform_resize_if_required() -> void
{   
    if (!resize_required()) [[likely]]
    {
        return;
    }

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
            // compute the new bucket index for this entry
            auto const new_index = (hasher(entry->key) & (new_capacity - 1));

            // grab a reference to the new bucket for this entry
            auto& new_bucket = new_buckets[new_index];

            // the insertion into the new bucket alters the `next` pointer for `entry`
            auto* tmp_next = entry->next;
            insert_into_bucket(new_bucket, entry);

            entry = tmp_next;
        }
    }

    delete[] buckets;

    buckets  = new_buckets;
    capacity = new_capacity;
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::resize_required() const -> bool
{
    // QUESTION: which of these is likely to lead to more effective short-circuit? 

    auto const load_factor_exceeded = (static_cast<double>(n_items) 
        / static_cast<double>(capacity)) > MAX_LOAD_FACTOR;
    
    auto const capacity_available = (capacity << 1) <= max_capacity; 
    
    return load_factor_exceeded && capacity_available;
}

template <
    typename KeyT, 
    typename ValueT, 
    typename Hasher>
auto Map<KeyT, ValueT, Hasher>::insert_into_bucket(
    Bucket& bucket, 
    Entry*  entry) -> void
{
    // ensure the `next` pointer for the new entry is nulled-out
    entry->next = nullptr;

    auto* current = bucket.first;
    if (nullptr == current)
    {
        bucket.first = entry;
    }
    else
    {
        // advance to the end of the bucket chain
        for (; current->next != nullptr; current = current->next);
        current->next = entry;
    }

    ++bucket.n_items;
}

// ----------------------------------------------------------------------------
// Misc. Helper Defintions

static std::size_t next_power_of_2(std::size_t n)
{
    // https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2

    n--;

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (8 == sizeof(n))
    {
        n |= n >> 32;
    }

    n++;

    return n;
}

#endif // MAP_HPP