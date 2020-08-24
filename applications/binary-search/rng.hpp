// rng.hpp
// Random number generation utilities.
//
// Adapted from original code by Gor Nishanov.
// https://github.com/GorNishanov/await/tree/master/2018_CppCon

#ifndef RNG_HPP
#define RNG_HPP

#include <random>

template <typename T>
struct rng
{
    std::mt19937_64                  generator;
    std::uniform_int_distribution<T> distribution;

    std::size_t const count;

    rng(unsigned int seed, T from, T to, std::size_t const count_)
        : distribution{from, to}
        , count{count_}
    {
        generator.seed(seed);
    }

    struct iterator
    {
        rng&   owner;
        size_t count;

        bool operator!= (iterator const& rhs) const
        {
            return rhs.count != count;
        }

        iterator& operator++()
        {
            --count;
            return *this;
        }

        T operator*()
        {
            return owner.distribution(owner.generator);
        }
    };

    iterator begin()
    {
        return {*this, count};
    }

    iterator end()
    {
        return {*this, 0};
    }
};

#endif // RNG_HPP