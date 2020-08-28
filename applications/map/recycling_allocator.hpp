// recycling_allocator.hpp
// Dead-simple recycling allocator.

#ifndef RECYLING_ALLOCATOR_HPP
#define RECYCLING_ALLOCATOR_HPP

#include <cstdlib>
#include <stdexcept>

struct RecyclingAllocator
{
    struct Header
    {
        Header*     next;
        std::size_t size;
    };

    Header* root;

public:
    RecyclingAllocator() 
        : root{nullptr} {}

    ~RecyclingAllocator()
    {
        auto* current = root;
        while (current != nullptr)
        {
            auto* next = current->next;
            ::free(current);
            current = next;
        }
    }

    // non-copyable
    RecyclingAllocator(RecyclingAllocator const&)            = delete;
    RecyclingAllocator& operator=(RecyclingAllocator const&) = delete;

    // non-movable
    RecyclingAllocator(RecyclingAllocator&&)            = delete;
    RecyclingAllocator& operator=(RecyclingAllocator&&) = delete;

    void* alloc(std::size_t n)
    {
        if (root != nullptr && root->size <= n)
        {
            auto* mem = root;
            root = root->next;
            return mem;
        }

        auto* ptr = ::malloc(n);
        if (nullptr == ptr)
        {
            throw std::bad_alloc{};
        }

        return ptr;
    }

    void free(void* ptr, std::size_t n)
    {
        auto new_entry = static_cast<Header*>(ptr);
        new_entry->size = n;
        new_entry->next = root;
        root = new_entry;
    }
};

// TODO: I hate having to define this globally with `inline` here,
// but the mechanics of providing a custom allocator for coroutine frame
// allocation are otherwise so messy its almost not worth the trouble.
inline RecyclingAllocator inline_recycling_allocator;

#endif // RECYCLING ALLOCATOR_HPP