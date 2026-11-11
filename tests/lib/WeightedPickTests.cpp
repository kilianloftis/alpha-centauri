#include "lib/WeightedPick.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <random>

using namespace ac;

TEST_CASE("PickWeightedIndex returns nullopt when there is nothing to pick", "[lib][weighted]")
{
    std::mt19937 rng(1);
    CHECK_FALSE(PickWeightedIndex(0, [](std::size_t) { return 1; }, rng).has_value());
    CHECK_FALSE(PickWeightedIndex(2, [](std::size_t) { return 0; }, rng).has_value());
}

TEST_CASE("PickWeightedIndex picks the only candidate", "[lib][weighted]")
{
    std::mt19937 rng(1);
    const auto picked = PickWeightedIndex(1, [](std::size_t) { return 4; }, rng);
    REQUIRE(picked.has_value());
    CHECK(*picked == 0);
}

TEST_CASE("PickWeightedIndex favors the heavier weight under a fixed seed", "[lib][weighted]")
{
    std::mt19937 rng(42);
    const int weights[] = {1, 9};
    int counts[2] = {0, 0};
    constexpr int kDraws = 1000;
    for (int i = 0; i < kDraws; ++i)
    {
        const auto picked =
            PickWeightedIndex(2, [&](std::size_t j) { return weights[j]; }, rng);
        REQUIRE(picked.has_value());
        ++counts[*picked];
    }
    CHECK(counts[1] > counts[0]);
    CHECK(counts[1] > 700);
}
