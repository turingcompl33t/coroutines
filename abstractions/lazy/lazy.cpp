// lazy.cpp

#include "lazy.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <cstdint>

lazy<int> massive_computation()
{
    using namespace std::chrono_literals;

    puts("performing massive computation...");
    std::this_thread::sleep_for(3s);
    puts("computation complete!");

    co_return 42;
}

int main(int argc, char* argv[])
{
    auto const get = argc > 1;

    auto l = massive_computation();
    if (get)
    {
        printf("computed: %d\n", l.get());
    }

    return EXIT_SUCCESS;
}