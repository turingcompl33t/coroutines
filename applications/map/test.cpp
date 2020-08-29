// test.cpp

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <memory>

#include "map.hpp"

TEST_CASE("map supports construction")
{
    SECTION("default construction")
    {
        Map<int, int> map{};
        REQUIRE(map.count() == 0);

        auto stats = map.stats();
        REQUIRE(stats.max_capacity == std::numeric_limits<std::size_t>::max());
    }

    SECTION("construction with explicit maximum capacity")
    {
        Map<int, int> map{32};
        REQUIRE(map.count() == 0);

        auto stats = map.stats();
        REQUIRE(stats.max_capacity == 32);
    }
}

TEST_CASE("map construction throws on invalid maximum capacity")
{
    std::unique_ptr<Map<int, int>> ptr{};
    REQUIRE_THROWS_AS(ptr.reset(new Map<int, int>{0}), std::runtime_error);
}

TEST_CASE("map supports insertion")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto result = map.insert(1, 1);
    
    REQUIRE(static_cast<bool>(result));
    REQUIRE(map.count() == 1);
}

TEST_CASE("map supports lookup")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto r1 = map.insert(1, 1);

    REQUIRE(static_cast<bool>(r1));
    REQUIRE(map.count() == 1);

    auto r2 = map.lookup(1);

    REQUIRE(static_cast<bool>(r2));
    REQUIRE(r2.get_key() == 1);
    REQUIRE(r2.get_value() == 1);
}

TEST_CASE("map supports update")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto r1 = map.insert(1, 1);

    REQUIRE(static_cast<bool>(r1));
    REQUIRE(map.count() == 1);

    auto r2 = map.lookup(1);

    REQUIRE(static_cast<bool>(r2));
    REQUIRE(r2.get_key() == 1);
    REQUIRE(r2.get_value() == 1);

    auto r3 = map.update(1, 2);

    REQUIRE(static_cast<bool>(r3));
    REQUIRE(r3.get_key() == 1);
    REQUIRE(r3.get_value() == 2);

    REQUIRE(map.count() == 1);
}

TEST_CASE("map supports removal")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    auto r1 = map.insert(1, 1);

    REQUIRE(static_cast<bool>(r1));
    REQUIRE(map.count() == 1);

    auto r2 = map.remove(1);

    REQUIRE(static_cast<bool>(r2));
    REQUIRE(map.count() == 0);

    REQUIRE(r2.get_key() == 1);
    REQUIRE(r2.get_value() == 1);

    REQUIRE(r2.take_key() == 1);
    REQUIRE(r2.take_value() == 1);
}

TEST_CASE("map correctly handles resize operations")
{
    Map<int, int> map{};
    REQUIRE(map.count() == 0);

    // trigger a resize (dirty knowledge)
    for (auto i = 0; i < 6; ++i)
    {
        auto const r = map.insert(i, i);
        REQUIRE(static_cast<bool>(r));
    }

    for (auto i = 0; i < 6; ++i)
    {
        auto const r = map.lookup(i);
        REQUIRE(static_cast<bool>(r));
    }
}