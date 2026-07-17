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

TEST_CASE("Negative move_cost fails at improvement parse time", "[move-cost]")
{
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "ac_negative_move_cost.json";
    {
        std::ofstream out(path);
        out << R"([
          { "id": "BadTerrain", "name": "Bad Terrain", "move_cost": -1, "effects": [] }
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
    CHECK_FALSE(flat.moveCostFragments.has_value());
    CHECK_FALSE(flat.moveCostOverrideFragments.has_value());

    const ImprovementConfig_t& rocky = fixture.improvements.Get("Rocky");
    REQUIRE(rocky.moveCostFragments.has_value());
    CHECK(*rocky.moveCostFragments == 2 * MovementConstants_t::k_moveFragmentsPerPoint);

    const ImprovementConfig_t& road = fixture.improvements.Get("Road");
    CHECK_FALSE(road.moveCostFragments.has_value());
    REQUIRE(road.moveCostOverrideFragments.has_value());
    CHECK(*road.moveCostOverrideFragments == MovementConstants_t::k_moveFragmentsPerPoint / 3);
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
    const auto costs = calc.ForUnit(unit, fixture.map);

    Tile& flat = fixture.At(3, 4);
    CHECK(costs.EntryTerms(flat).costFragments == k_point);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(costs.EntryTerms(rocky).costFragments == 2 * k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    CHECK(costs.EntryTerms(fungus).costFragments == 3 * k_point);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    CHECK(costs.EntryTerms(rockyFungus).costFragments == 3 * k_point);

    Tile& road = fixture.At(4, 5);
    road.SetRockiness(Rockiness_t::Rocky);
    road.AddImprovement(fixture.improvements.Get("Road"));
    CHECK(costs.EntryTerms(road).costFragments == k_point / 3);

    Tile& tube = fixture.At(5, 5);
    tube.SetHasFungus(true);
    tube.AddImprovement(fixture.improvements.Get("MagTube"));
    CHECK(costs.EntryTerms(tube).costFragments == 0);

    // Override replaces max cost even when the override is numerically higher.
    ImprovementConfig_t highOverride;
    highOverride.id = "TestHighOverride";
    highOverride.moveCostOverrideFragments = 5 * k_point;
    Tile& overridden = fixture.At(6, 5);
    overridden.SetRockiness(Rockiness_t::Rocky); // move_cost 2
    overridden.AddImprovement(highOverride);
    CHECK(costs.EntryTerms(overridden).costFragments == 5 * k_point);
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
    const auto hoverCosts = calc.ForUnit(hoverUnit, fixture.map);
    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK(hoverCosts.EntryTerms(rocky).costFragments == k_point);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    // Fungus is not difficult terrain — full price still applies unless treatFungusAsRoad.
    CHECK(hoverCosts.EntryTerms(fungus).costFragments == 3 * k_point);
    CHECK(hoverCosts.EntryTerms(fungus).bRequiresFullCost);

    Unit& nativeUnit = fixture.MakeUnit(faction, 4, 5, {"test_chassis", "treat_fungus_as_road"});
    const auto nativeCosts = calc.ForUnit(nativeUnit, fixture.map);
    CHECK(nativeCosts.EntryTerms(fungus).costFragments == k_point / 3);

    Tile& rockyFungus = fixture.At(7, 4);
    rockyFungus.SetRockiness(Rockiness_t::Rocky);
    rockyFungus.SetHasFungus(true);
    Unit& bothUnit = fixture.MakeUnit(faction, 4, 6,
                                      {"test_chassis", "ignores_difficult_terrain",
                                       "treat_fungus_as_road"});
    const auto bothCosts = calc.ForUnit(bothUnit, fixture.map);
    CHECK(bothCosts.EntryTerms(rockyFungus).costFragments == k_point / 3);
}

TEST_CASE("EntryTerms resolve the fungus entry rules", "[move-cost][fungus]")
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
    const auto costs = calc.ForUnit(unit, fixture.map);

    Tile& flat = fixture.At(3, 4);
    const EntryTerms_t flatTerms = costs.EntryTerms(flat);
    CHECK_FALSE(flatTerms.bRequiresFullCost);
    CHECK_FALSE(flatTerms.bEndsTurn);

    Tile& emptyFungus = fixture.At(5, 4);
    emptyFungus.SetHasFungus(true);
    const EntryTerms_t emptyTerms = costs.EntryTerms(emptyFungus);
    CHECK(emptyTerms.costFragments == 3 * k_point);
    CHECK(emptyTerms.bRequiresFullCost);
    CHECK(emptyTerms.bEndsTurn);

    // A friendly occupant waives the banking requirement but not the forced end of turn.
    Tile& friendFungus = fixture.At(6, 4);
    friendFungus.SetHasFungus(true);
    fixture.MakeUnit(faction, 6, 4, {"test_chassis"});
    const EntryTerms_t friendTerms = costs.EntryTerms(friendFungus);
    CHECK_FALSE(friendTerms.bRequiresFullCost);
    CHECK(friendTerms.bEndsTurn);

    // Road built on fungus negates the entry rules and overrides the cost.
    Tile& roadFungus = fixture.At(7, 4);
    roadFungus.SetHasFungus(true);
    roadFungus.AddImprovement(fixture.improvements.Get("Road"));
    const EntryTerms_t roadTerms = costs.EntryTerms(roadFungus);
    CHECK(roadTerms.costFragments == k_point / 3);
    CHECK_FALSE(roadTerms.bRequiresFullCost);
    CHECK_FALSE(roadTerms.bEndsTurn);

    // TreatFungusAsRoad behaves as if the tile had a Road.
    Unit& native = fixture.MakeUnit(faction, 4, 6, {"test_chassis", "treat_fungus_as_road"});
    const auto nativeCosts = calc.ForUnit(native, fixture.map);
    const EntryTerms_t nativeTerms = nativeCosts.EntryTerms(emptyFungus);
    CHECK(nativeTerms.costFragments == k_point / 3);
    CHECK_FALSE(nativeTerms.bRequiresFullCost);
    CHECK_FALSE(nativeTerms.bEndsTurn);
}

TEST_CASE("PlannedCostFragments values end-turn entries in whole turns", "[move-cost][fungus]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    Faction& faction = fixture.MakeFaction();
    for (const auto& pTile : fixture.map.GetTiles())
    {
        faction.GetExploredMap().Mark(*pTile);
    }

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMovementPoints() == 2);
    const auto costs = calc.ForUnit(unit, fixture.map);

    Tile& emptyFungus = fixture.At(5, 4);
    emptyFungus.SetHasFungus(true);
    // M=2 banking cost 3: two whole turns of movement.
    CHECK(costs.PlannedCostFragments(emptyFungus) == 4 * k_point);

    Tile& friendFungus = fixture.At(6, 4);
    friendFungus.SetHasFungus(true);
    fixture.MakeUnit(faction, 6, 4, {"test_chassis"});
    // Immediate entry still ends the turn: exactly one allotment.
    CHECK(costs.PlannedCostFragments(friendFungus) == 2 * k_point);

    Unit& slow = fixture.MakeUnit(faction, 4, 5, {"test_slow_chassis"});
    REQUIRE(slow.GetMovementPoints() == 1);
    const auto slowCosts = calc.ForUnit(slow, fixture.map);
    CHECK(slowCosts.PlannedCostFragments(emptyFungus) == 3 * k_point);

    // Non-end-turn entries plan at their entry cost.
    Unit& native = fixture.MakeUnit(faction, 4, 6, {"test_chassis", "treat_fungus_as_road"});
    const auto nativeCosts = calc.ForUnit(native, fixture.map);
    CHECK(nativeCosts.PlannedCostFragments(emptyFungus) == k_point / 3);
}

TEST_CASE("PlannedCostFragments hides tile detail under shroud", "[move-cost]")
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
    const auto costs = calc.ForUnit(unit, fixture.map);

    Tile& fungus = fixture.At(6, 4);
    fungus.SetHasFungus(true);
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(fungus));
    CHECK(costs.PlannedCostFragments(fungus) == k_point);

    faction.GetExploredMap().Mark(fungus);
    CHECK(costs.PlannedCostFragments(fungus) == 4 * k_point);
}
