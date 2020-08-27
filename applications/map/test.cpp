// test.cpp

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "map.hpp"

TEST_CASE("map supports construction")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);
}

TEST_CASE("map supports insertion")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto result = map.insert(1, 1);
    
    REQUIRE(result.successful());
    REQUIRE(map.count() == 1);
}

TEST_CASE("map supports insertion and subsequent synchonous lookup")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto r1 = map.insert(1, 1);

    REQUIRE(r1.successful());
    REQUIRE(map.count() == 1);

    auto r2 = map.sync_lookup(1);

    REQUIRE(static_cast<bool>(r2));
    REQUIRE(r2.get_key() == 1);
    REQUIRE(r2.get_value() == 1);
}

TEST_CASE("map correctly handles resize operations")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    // trigger a resize (dirty knowledge)
    for (auto i = 0; i < 5; ++i)
    {
        auto const r = map.insert(i, i);
        REQUIRE(r.successful());
    }

    for (auto i = 0; i < 5; ++i)
    {
        auto const r = map.sync_lookup(i);
        REQUIRE(static_cast<bool>(r));
    }
}

TEST_CASE("map supports asynchronous lookup with coroutines")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto const insertion = map.insert(1, 1);
    REQUIRE(insertion.successful());

    auto const lookup = sequential_lookup(map, 1);
    REQUIRE(static_cast<bool>(lookup));
}