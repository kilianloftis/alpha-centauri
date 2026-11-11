#include "lib/RandomRoll.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <random>

using namespace ac;

TEST_CASE("PickUniformIndex rejects an empty range", "[lib][random]")
{
    std::mt19937 rng(1);
    CHECK_THROWS_WITH(PickUniformIndex(0, rng),
                      Catch::Matchers::ContainsSubstring("positive"));
}

TEST_CASE("PickUniformIndex returns the only index without drawing", "[lib][random]")
{
    std::mt19937 rng(1);
    const std::mt19937 before = rng;
    CHECK(PickUniformIndex(1, rng) == 0);
    CHECK(rng == before);
}

TEST_CASE("PickUniformIndex stays inside the range under a fixed seed", "[lib][random]")
{
    std::mt19937 rng(42);
    constexpr std::size_t kCount = 5;
    for (int i = 0; i < 100; ++i)
    {
        const std::size_t picked = PickUniformIndex(kCount, rng);
        CHECK(picked < kCount);
    }
}

TEST_CASE("RollInclusive rejects an inverted range", "[lib][random]")
{
    std::mt19937 rng(1);
    CHECK_THROWS_WITH(RollInclusive(3, 2, rng),
                      Catch::Matchers::ContainsSubstring("max must be"));
}

TEST_CASE("RollInclusive returns a fixed bound without drawing", "[lib][random]")
{
    std::mt19937 rng(1);
    const std::mt19937 before = rng;
    CHECK(RollInclusive(7, 7, rng) == 7);
    CHECK(rng == before);
}

TEST_CASE("RollInclusive stays inside the range under a fixed seed", "[lib][random]")
{
    std::mt19937 rng(42);
    for (int i = 0; i < 100; ++i)
    {
        const int rolled = RollInclusive(3, 8, rng);
        CHECK(rolled >= 3);
        CHECK(rolled <= 8);
    }
}
