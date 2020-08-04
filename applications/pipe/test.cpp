

#include "future.hpp"

#include <cstdlib>
#include <iostream>

std::future<int> foo()
{
    co_return 1;
}

int main()
{
    auto f = foo();
    std::cout << f.get() << '\n';

    return EXIT_SUCCESS;
}