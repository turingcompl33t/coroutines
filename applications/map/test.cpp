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

TEST_CASE("map supports insertion and subsequent lookup")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto r1 = map.insert(1, 1);

    REQUIRE(r1.successful());
    REQUIRE(map.count() == 1);

    auto r2 = map.lookup(1);

    REQUIRE(static_cast<bool>(r2));
    REQUIRE(r2.get_key() == 1);
    REQUIRE(r2.get_value() == 1);
}