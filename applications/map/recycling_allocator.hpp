// recycling_allocator.hpp
// Dead-simple recycling allocator.

#ifndef RECYLING_ALLOCATOR_HPP
#define RECYCLING_ALLOCATOR_HPP

#include <cstdlib>

struct RecyclingAllocator
{
    struct Header
    {
        Header*     next;
        std::size_t size;
    };

    Header* root{nullptr};

    std::size_t last_size_allocated = 0;
    std::size_t total               = 0;
    size_t alloc_count         = 0;

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

    void* alloc(std::size_t n)
    {
        if (root != nullptr && root->size <= n)
        {
            void* mem = root;
            root = root->next;
            return mem;
        }

        return ::malloc(n);
    }

    void free(void* ptr, std::size_t n)
    {
        auto new_entry = static_cast<Header*>(ptr);
        new_entry->size = n;
        new_entry->next = root;
        root = new_entry;
    }
};

inline RecyclingAllocator inline_recycling_allocator;

#endif // RECYCLING ALLOCATOR_HPP