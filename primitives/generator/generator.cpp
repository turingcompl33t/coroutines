// generator.cpp
// Driver for simple integer generator.

#include "generator.hpp"

#include <cstdlib>
#include <iostream>

// inifinite stream of integers
generator integers()
{
    for (size_t i = 0;;++i)
    {
        co_yield i;
    }
}

int main()
{
    auto ints = integers();
    for (size_t i = 0; i < 5; ++i, ints.next())
    {
        std::cout << ints.value() << '\n';
    }   

    return EXIT_SUCCESS;
}