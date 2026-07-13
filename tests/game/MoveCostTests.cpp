#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "GameFixtures.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "lib/Rational.h"

using namespace ac;
using namespace actest;

TEST_CASE("Rational parses ints and fraction strings", "[move-cost][rational]")
{
    CHECK(Rational_t::FromInt(2).ScaledInt(360) == 720);
    CHECK(Rational_t::Parse("1/3").ScaledInt(360) == 120);
    CHECK(Rational_t::ParseJson(nlohmann::json(0)).ScaledInt(360) == 0);
    CHECK(Rational_t::ParseJson(nlohmann::json("1/3")).ScaledInt(360) == 120);
    CHECK_THROWS(Rational_t::Parse("1/7").ScaledInt(360));
    CHECK_THROWS(Rational_t::Parse("1/0"));
}

TEST_CASE("Improvement move_cost defaults to 1 when omitted", "[move-cost]")
{
    WorldFixture fixture;
    const ImprovementConfig_t& flat = fixture.improvements.Get("Flat");
    CHECK(flat.moveCost.numerator == 1);
    CHECK(flat.moveCost.denominator == 1);
    CHECK_FALSE(flat.moveCostOverride.has_value());

    const ImprovementConfig_t& rocky = fixture.improvements.Get("Rocky");
    CHECK(rocky.moveCost.numerator == 2);
    CHECK(rocky.moveCost.denominator == 1);

    const ImprovementConfig_t& road = fixture.improvements.Get("Road");
    REQUIRE(road.moveCostOverride.has_value());
    CHECK(road.moveCostOverride->ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint)
          == MovementConstants_t::k_moveFragmentsPerPoint / 3);
}

TEST_CASE("MoveCostCalculator takes max cost or min override", "[move-cost]")
{
    WorldFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    const UnitMoveProfile_t land{};
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

    Tile& flat = fixture.At(4, 4);
    CHECK(calc.ComputeFragments(flat, land) == k_point);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(calc.ComputeFragments(rocky, land) == 2 * k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    CHECK(calc.ComputeFragments(fungus, land) == 3 * k_point);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    CHECK(calc.ComputeFragments(rockyFungus, land) == 3 * k_point);

    Tile& road = fixture.At(4, 5);
    road.SetRockiness(Rockiness_t::Rocky);
    road.AddImprovement(fixture.improvements.Get("Road"));
    CHECK(calc.ComputeFragments(road, land) == k_point / 3);

    Tile& tube = fixture.At(5, 5);
    tube.SetHasFungus(true);
    tube.AddImprovement(fixture.improvements.Get("MagTube"));
    CHECK(calc.ComputeFragments(tube, land) == 0);
}

TEST_CASE("MoveCostCalculator honours UnitMoveProfile flags", "[move-cost]")
{
    WorldFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

    UnitMoveProfile_t ignoresDifficult;
    ignoresDifficult.ignoresDifficultTerrain = true;

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(calc.ComputeFragments(rocky, ignoresDifficult) == k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    // Fungus is not difficult terrain — still costs 3 unless treatFungusAsRoad.
    CHECK(calc.ComputeFragments(fungus, ignoresDifficult) == 3 * k_point);

    UnitMoveProfile_t fungusAsRoad;
    fungusAsRoad.treatFungusAsRoad = true;
    CHECK(calc.ComputeFragments(fungus, fungusAsRoad) == k_point / 3);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    UnitMoveProfile_t both;
    both.ignoresDifficultTerrain = true;
    both.treatFungusAsRoad = true;
    CHECK(calc.ComputeFragments(rockyFungus, both) == k_point / 3);
}
