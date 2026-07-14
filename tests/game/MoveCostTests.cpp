#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "GameFixtures.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "lib/Rational.h"

#include <filesystem>
#include <fstream>

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

TEST_CASE("Invalid move_cost fraction fails at improvement parse time", "[move-cost]")
{
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "ac_bad_move_cost.json";
    {
        std::ofstream out(path);
        out << R"([
          { "id": "BadRoad", "name": "Bad Road", "move_cost_override": "1/7", "effects": [] }
        ])";
    }

    ImprovementConfigParser parser;
    CHECK_THROWS(parser.ParseConfig(path.string()));
    fs::remove(path);
}

TEST_CASE("Improvement move_cost is optional when omitted", "[move-cost]")
{
    WorldFixture fixture;
    const ImprovementConfig_t& flat = fixture.improvements.Get("Flat");
    CHECK_FALSE(flat.moveCost.has_value());
    CHECK_FALSE(flat.moveCostOverride.has_value());

    const ImprovementConfig_t& rocky = fixture.improvements.Get("Rocky");
    REQUIRE(rocky.moveCost.has_value());
    CHECK(rocky.moveCost->numerator == 2);
    CHECK(rocky.moveCost->denominator == 1);

    const ImprovementConfig_t& road = fixture.improvements.Get("Road");
    CHECK_FALSE(road.moveCost.has_value());
    REQUIRE(road.moveCostOverride.has_value());
    CHECK(road.moveCostOverride->ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint)
          == MovementConstants_t::k_moveFragmentsPerPoint / 3);
}

TEST_CASE("MoveCostCalculator takes max cost or min override", "[move-cost]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    Tile& flat = fixture.At(3, 4);
    CHECK(calc.ComputeFragments(unit, flat, fixture.map) == k_point);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(calc.ComputeFragments(unit, rocky, fixture.map) == 2 * k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    CHECK(calc.ComputeFragments(unit, fungus, fixture.map) == 3 * k_point);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    CHECK(calc.ComputeFragments(unit, rockyFungus, fixture.map) == 3 * k_point);

    Tile& road = fixture.At(4, 5);
    road.SetRockiness(Rockiness_t::Rocky);
    road.AddImprovement(fixture.improvements.Get("Road"));
    CHECK(calc.ComputeFragments(unit, road, fixture.map) == k_point / 3);

    Tile& tube = fixture.At(5, 5);
    tube.SetHasFungus(true);
    tube.AddImprovement(fixture.improvements.Get("MagTube"));
    CHECK(calc.ComputeFragments(unit, tube, fixture.map) == 0);

    // Override replaces max cost even when the override is numerically higher.
    ImprovementConfig_t highOverride;
    highOverride.id = "TestHighOverride";
    highOverride.moveCostOverride = Rational_t::FromInt(5);
    Tile& overridden = fixture.At(6, 5);
    overridden.SetRockiness(Rockiness_t::Rocky); // move_cost 2
    overridden.AddImprovement(highOverride);
    CHECK(calc.ComputeFragments(unit, overridden, fixture.map) == 5 * k_point);
}

TEST_CASE("MoveCostCalculator honours UnitMoveProfile flags", "[move-cost]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    Faction& faction = fixture.MakeFaction();

    Unit& hoverUnit = fixture.MakeUnit(faction, 4, 4,
                                       {"test_chassis", "ignores_difficult_terrain"});
    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(calc.ComputeFragments(hoverUnit, rocky, fixture.map) == k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    // Fungus is not difficult terrain — still costs 3 unless treatFungusAsRoad.
    CHECK(calc.ComputeFragments(hoverUnit, fungus, fixture.map) == 3 * k_point);

    Unit& nativeUnit = fixture.MakeUnit(faction, 4, 5, {"test_chassis", "treat_fungus_as_road"});
    CHECK(calc.ComputeFragments(nativeUnit, fungus, fixture.map) == k_point / 3);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    Unit& bothUnit = fixture.MakeUnit(faction, 4, 6,
                                      {"test_chassis", "ignores_difficult_terrain",
                                       "treat_fungus_as_road"});
    CHECK(calc.ComputeFragments(bothUnit, rockyFungus, fixture.map) == k_point / 3);
}

TEST_CASE("ComputeFragments treats friendly fungus as default cost", "[move-cost][fungus]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    Tile& emptyFungus = fixture.At(5, 4);
    emptyFungus.SetHasFungus(true);
    CHECK(calc.ComputeFragments(unit, emptyFungus, fixture.map) == 3 * k_point);

    Tile& friendFungus = fixture.At(6, 4);
    friendFungus.SetHasFungus(true);
    fixture.MakeUnit(faction, 6, 4, {"test_chassis"});
    CHECK(calc.ComputeFragments(unit, friendFungus, fixture.map) == k_point);
}
