// dev_null_iterator.hpp
// An output iterator that just consumes its input.

#ifndef DEV_NULL_ITERATOR_HPP
#define DEV_NULL_ITERATOR_HPP

#include <iterator>

// an output iterator that consumes its input
class DevNullIterator
{
public:
    using iterator_category = std::output_iterator_tag;

    DevNullIterator& operator++() { return *this; }
    DevNullIterator& operator*() { return *this; }

    template <typename T>
    DevNullIterator& operator=(T const&)
    {
        return *this;
    } 
};

#endif // DEV_NULL_ITERATOR_HPP