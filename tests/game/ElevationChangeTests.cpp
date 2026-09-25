#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/ElevationChange.h"
#include "game/map/ElevationRulesConfigParser.h"
#include "game/map/WorldGenPresetConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/CombatResolver.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/units/UnitDomain.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/effects/TriggeredEffectDispatch.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;
using Catch::Matchers::ContainsSubstring;

namespace
{

ElevationRulesConfig_t ShippingRules_()
{
    ElevationRulesConfig_t rules;
    rules.minElevationMeters = -4000;
    rules.maxElevationMeters = 4000;
    rules.oceanLevelMeters = 0;
    rules.oceanShelfMeters = -2000;
    rules.levelMinMeters = 500;
    rules.levelMaxMeters = 1500;
    rules.maxAdjacentDifferenceMeters = 1500;
    rules.referenceLevelMeters = 1000;
    rules.spreadAltitudeLimitMeters = 1000;
    return rules;
}

void WriteTempRules_(const std::filesystem::path& rPath, const std::string& rBody)
{
    std::ofstream out(rPath);
    out << rBody;
}

Tile& At_(WorldMap& rMap, int x, int y)
{
    Tile* pTile = rMap.GetTile(x, y);
    REQUIRE(pTile);
    return *pTile;
}

int UnitCount_(const Faction& rFaction)
{
    return static_cast<int>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

struct OrderHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    explicit OrderHarness_(FactionFixture& rFixture)
        : moveCosts(rFixture.improvements)
        , steps(rFixture.map, *rFixture.ctx)
        , pathfinder(moveCosts, steps, rFixture.map)
        , rng(k_TestRngSeed)
        , orders(moveCosts, steps, rFixture.map, *rFixture.ctx, pathfinder, rFixture.morale(), rng)
    {
    }
};

} // namespace

TEST_CASE("Elevation rules reject missing and invalid scalars", "[map][elevation]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_bad_elevation_rules.json";

    WriteTempRules_(path, R"({
        "ocean_level_meters": 0,
        "ocean_shelf_meters": -2000,
        "level_min_meters": 500,
        "level_max_meters": 1500,
        "reference_level_meters": 1000,
        "spread_altitude_limit_meters": 1000
    })");
    CHECK_THROWS_WITH(ElevationRulesConfigParser{}.ParseConfig(path.string()),
                      ContainsSubstring("max_adjacent_difference_meters"));

    WriteTempRules_(path, R"({
        "ocean_level_meters": 0,
        "ocean_shelf_meters": -2000,
        "level_min_meters": 1500,
        "level_max_meters": 500,
        "max_adjacent_difference_meters": 1500,
        "reference_level_meters": 1000,
        "spread_altitude_limit_meters": 1000
    })");
    CHECK_THROWS_WITH(ElevationRulesConfigParser{}.ParseConfig(path.string()),
                      ContainsSubstring("level_max_meters"));

    WriteTempRules_(path, R"({
        "ocean_level_meters": 0,
        "ocean_shelf_meters": 0,
        "level_min_meters": 500,
        "level_max_meters": 1500,
        "max_adjacent_difference_meters": 1500,
        "reference_level_meters": 1000,
        "spread_altitude_limit_meters": 1000
    })");
    CHECK_THROWS_WITH(ElevationRulesConfigParser{}.ParseConfig(path.string()),
                      ContainsSubstring("ocean_shelf_meters"));

    WriteTempRules_(path, R"({
        "ocean_level_meters": 0,
        "ocean_shelf_meters": -2000,
        "level_min_meters": 500,
        "level_max_meters": 1500,
        "max_adjacent_difference_meters": 0,
        "reference_level_meters": 1000,
        "spread_altitude_limit_meters": 1000
    })");
    CHECK_THROWS_WITH(ElevationRulesConfigParser{}.ParseConfig(path.string()),
                      ContainsSubstring("max_adjacent_difference_meters"));

    WriteTempRules_(path, R"({
        "ocean_level_meters": 0,
        "ocean_shelf_meters": -2000,
        "level_min_meters": 500,
        "level_max_meters": 1500,
        "max_adjacent_difference_meters": 1500,
        "reference_level_meters": 1000,
        "spread_altitude_limit_meters": -500
    })");
    CHECK_THROWS_WITH(ElevationRulesConfigParser{}.ParseConfig(path.string()),
                      ContainsSubstring("spread_altitude_limit_meters"));

    std::filesystem::remove(path);
}

TEST_CASE("A preset's elevation range is the world storage range", "[map][elevation][worldgen]")
{
    ElevationRulesConfig_t rules;
    rules.oceanLevelMeters = 0;
    rules.oceanShelfMeters = -2000;
    rules.spreadAltitudeLimitMeters = 1000;

    WorldGenPresetConfig_t preset;
    preset.id = "highlands";
    preset.minElevation = -3000;
    preset.maxElevation = 2500;

    WorldGenPresetConfigParser::ApplyElevationRange(rules, preset);
    CHECK(rules.minElevationMeters == -3000);
    CHECK(rules.maxElevationMeters == 2500);

    preset.maxElevation = 500;
    CHECK_THROWS_WITH(WorldGenPresetConfigParser::ApplyElevationRange(rules, preset),
                      ContainsSubstring("spread_altitude_limit_meters"));

    preset.maxElevation = 2500;
    preset.minElevation = -1000;
    CHECK_THROWS_WITH(WorldGenPresetConfigParser::ApplyElevationRange(rules, preset),
                      ContainsSubstring("ocean_shelf_meters"));
}

TEST_CASE("Elevation delta pulls neighbors only past the slope limit", "[map][elevation]")
{
    const ElevationRulesConfig_t rules = ShippingRules_();
    WorldMap map(9, 9, rules);

    SECTION("a neighbor already within the limit stays")
    {
        At_(map, 4, 4).SetElevation(0);
        At_(map, 4, 5).SetElevation(0);
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, 1000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == 1000);
        CHECK(At_(map, 4, 5).GetElevation() == 0);
    }

    SECTION("a neighbor too low is raised to origin minus the limit")
    {
        At_(map, 4, 4).SetElevation(0);
        At_(map, 4, 5).SetElevation(-2000);
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, 1000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == 1000);
        CHECK(At_(map, 4, 5).GetElevation() == 1000 - rules.maxAdjacentDifferenceMeters);
    }

    SECTION("a neighbor too high is lowered to origin plus the limit")
    {
        At_(map, 4, 4).SetElevation(3000);
        At_(map, 4, 5).SetElevation(3000);
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, -2000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == 1000);
        CHECK(At_(map, 4, 5).GetElevation() == 1000 + rules.maxAdjacentDifferenceMeters);
    }

    SECTION("the next ring moves when the first correction still breaks the limit")
    {
        At_(map, 4, 4).SetElevation(0);
        At_(map, 4, 5).SetElevation(0);
        At_(map, 4, 6).SetElevation(0);
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, 4500, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == rules.maxElevationMeters);
        CHECK(At_(map, 4, 5).GetElevation()
              == rules.maxElevationMeters - rules.maxAdjacentDifferenceMeters);
        CHECK(At_(map, 4, 6).GetElevation()
              == At_(map, 4, 5).GetElevation() - rules.maxAdjacentDifferenceMeters);
    }

    SECTION("relaxation can pull a land neighbor under ocean level")
    {
        At_(map, 4, 4).SetElevation(-1000);
        At_(map, 4, 5).SetElevation(1000);
        REQUIRE(At_(map, 4, 5).IsLand());
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, -2000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == -3000);
        CHECK(At_(map, 4, 5).GetElevation()
              == -3000 + rules.maxAdjacentDifferenceMeters);
        // The pull crossed ocean level, so the tile is sea now and its depth band followed.
        CHECK(At_(map, 4, 5).IsWater());
        CHECK(At_(map, 4, 5).HasFeature("OceanShelf"));
    }

    SECTION("relaxation can lift a sea neighbor above ocean level")
    {
        At_(map, 4, 4).SetElevation(0);
        At_(map, 4, 5).SetElevation(-3000);
        REQUIRE(At_(map, 4, 5).IsWater());
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, 2000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 5).GetElevation() == 2000 - rules.maxAdjacentDifferenceMeters);
        CHECK(At_(map, 4, 5).IsLand());
    }

    SECTION("a cliff the walk never reaches stays")
    {
        At_(map, 4, 4).SetElevation(0);
        At_(map, 0, 0).SetElevation(3000);
        REQUIRE(ApplyElevationDelta(At_(map, 4, 4), map, 1000, rules, rules.minElevationMeters,
                                    rules.maxElevationMeters));
        CHECK(At_(map, 4, 4).GetElevation() == 1000);
        CHECK(At_(map, 0, 0).GetElevation() == 3000);
    }
}

TEST_CASE("Former raise and an earthquake both clamp at max map elevation",
          "[map][elevation]")
{
    ElevationRulesConfig_t rules = ShippingRules_();
    rules.levelMinMeters = 1500;
    rules.levelMaxMeters = 1500;
    WorldMap map(5, 5, rules);

    At_(map, 2, 2).SetElevation(3000);
    REQUIRE(ApplyElevationDelta(At_(map, 2, 2), map, 2000, rules, rules.minElevationMeters,
                                rules.maxElevationMeters));
    CHECK(At_(map, 2, 2).GetElevation() == rules.maxElevationMeters);

    At_(map, 2, 2).SetElevation(3000);
    std::mt19937 rng(1);
    REQUIRE(ApplyEarthquake(At_(map, 2, 2), map, 4, rng, rules));
    CHECK(At_(map, 2, 2).GetElevation() == rules.maxElevationMeters);

    At_(map, 2, 3).SetElevation(0);
    CHECK_FALSE(ApplyEarthquake(At_(map, 2, 3), map, 0, rng, rules));
    CHECK(At_(map, 2, 3).GetElevation() == 0);
}

TEST_CASE("A special's requires_chassis list is enforced", "[unit][elevation]")
{
    UnitComponentRegistry specials;
    specials.Load(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/unit_components/specials.json");
    const UnitComponentConfig_t& rPayload = specials.Get("Tectonic_Payload");
    REQUIRE_FALSE(rPayload.requiresChassis.empty());

    UnitComponentConfig_t allowedChassis;
    allowedChassis.id = rPayload.requiresChassis.front();
    allowedChassis.type = "chassis";
    allowedChassis.domain = UnitDomain_t::Orbital;

    UnitComponentConfig_t otherChassis;
    otherChassis.id = rPayload.requiresChassis.front() + "_other";
    otherChassis.type = "chassis";
    otherChassis.domain = UnitDomain_t::Land;

    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };

    const std::unordered_map<std::string, const UnitComponentConfig_t*> onAllowed = {
        {"weapon", &rPayload},
        {"chassis", &allowedChassis},
    };
    CHECK_NOTHROW(UnitDesign(slots, onAllowed));

    const std::unordered_map<std::string, const UnitComponentConfig_t*> onOther = {
        {"weapon", &rPayload},
        {"chassis", &otherChassis},
    };
    CHECK_THROWS_WITH(UnitDesign(slots, onOther), ContainsSubstring("Tectonic_Payload"));

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_bad_chassis_requirement.json";
    {
        std::ofstream out(path);
        out << R"([{
            "id": "Orphan_Payload",
            "name": "Orphan Payload",
            "type": "weapon",
            "requires_chassis": ["NotAChassis"]
        }])";
    }
    UnitComponentRegistry orphan;
    orphan.Load(path.string());
    CHECK_THROWS_WITH(ValidateComponentChassisRequirements(orphan),
                      ContainsSubstring("NotAChassis"));
    std::filesystem::remove(path);

    UnitComponentRegistry shipping;
    shipping.Load(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/unit_components");
    CHECK_NOTHROW(ValidateComponentChassisRequirements(shipping));
}

TEST_CASE("Attacking with a detonation weapon does not raise the target tile",
          "[unit][elevation]")
{
    UnitComponentConfig_t weapon;
    weapon.id = "tectonic_test_weapon";
    weapon.type = "weapon";
    EffectPool pool;
    weapon.effects.push_back(
        pool.StatMod(StatId_t::Attack, 4.0, ModifierOp_t::Add, EffectScope_t::ThisUnit));
    TriggeredEffectConfig_t quake;
    quake.effect = EarthquakeEffect_t{.levels = 2};
    weapon.onDetonateEffects.push_back(std::move(quake));

    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    const UnitComponentConfig_t* pChassis = fixture.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
        {"weapon", &weapon},
        {"chassis", pChassis},
    };
    fixture.designs.emplace_back(slots, assigned);
    Unit& attacker = player.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, fixture.designs.back(), fixture.map.GetUnitPositions(),
        fixture.At(4, 4));

    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    const int targetElevation = fixture.At(5, 4).GetElevation();

    OrderHarness_ harness(fixture);
    const auto result = harness.orders.TryAttack(attacker, fixture.At(5, 4));
    REQUIRE(result.has_value());
    CHECK(fixture.At(5, 4).GetElevation() == targetElevation);
}

namespace
{

// A GameState over its own 9x9 map: ApplyDetonation resolves the tile through the state's
// world map, so the unit must live on that map rather than FactionFixture's.
struct DetonateGame_
{
    FactionFixture fixtures;
    // Declared before designs, which hold pointers into warhead, which the units' designs
    // outlive only if these are destroyed last.
    EffectPool pool;
    UnitComponentConfig_t warhead;
    std::deque<UnitDesign> designs;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pFaction = nullptr;

    DetonateGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, actest::k_TestRngSeed);
        pFaction = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed));
    }

    // A missile-shaped design: the warhead detonates for levelsStat and spends the carrier.
    Unit& MakeWarhead(int x, int y, int reactorLevels)
    {
        warhead.id = "test_warhead";
        warhead.type = "weapon";
        TriggeredEffectConfig_t quake;
        quake.effect = EarthquakeEffect_t{.levelsStat = StatId_t::EarthquakeLevels};
        warhead.onDetonateEffects.push_back(std::move(quake));
        TriggeredEffectConfig_t spend;
        spend.effect = DestroyUnitEffect_t{};
        warhead.onDetonateEffects.push_back(std::move(spend));
        warhead.effects.push_back(pool.StatMod(StatId_t::EarthquakeLevels,
                                               static_cast<double>(reactorLevels),
                                               ModifierOp_t::Add, EffectScope_t::ThisUnit));

        const UnitComponentConfig_t* pChassis = fixtures.unitComponents.Find("test_chassis");
        REQUIRE(pChassis);
        const std::vector<UnitSlotConfig_t> slots = {
            {.id = "weapon", .displayName = "Weapon", .componentType = "weapon",
             .required = true},
            {.id = "chassis", .displayName = "Chassis", .componentType = "chassis",
             .required = true},
        };
        const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
            {"weapon", &warhead},
            {"chassis", pChassis},
        };
        designs.emplace_back(slots, assigned);

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return pFaction->GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile, /*pHome=*/nullptr,
            /*pProducedAt=*/nullptr);
    }
};

} // namespace

TEST_CASE("Detonating a warhead raises its own tile and spends the carrier",
          "[unit][elevation][detonate]")
{
    DetonateGame_ game;
    Unit& warhead = game.MakeWarhead(4, 4, /*reactorLevels=*/2);
    REQUIRE(UnitCount_(*game.pFaction) == 1);
    REQUIRE(UnitCanDetonate(warhead));
    REQUIRE(warhead.GetStat(StatId_t::EarthquakeLevels) == 2);

    const int before = At_(game.pState->GetWorldMap(), 4, 4).GetElevation();
    REQUIRE(ApplyDetonation(*game.pState, warhead));

    // Two levels of [500, 1500] each, so the origin rises by at least 1000m.
    CHECK(At_(game.pState->GetWorldMap(), 4, 4).GetElevation() >= before + 1000);
    // The neighbor relaxation kept the slope inside the configured limit.
    const int origin = At_(game.pState->GetWorldMap(), 4, 4).GetElevation();
    CHECK(origin - At_(game.pState->GetWorldMap(), 4, 5).GetElevation()
          <= actest::TestMapRules().maxAdjacentDifferenceMeters);
    // The DestroyUnit entry spent the missile.
    CHECK(UnitCount_(*game.pFaction) == 0);
}

TEST_CASE("An earthquake that raises a sea tile onto land removes sea-domain improvements",
          "[unit][elevation][surface]")
{
    DetonateGame_ game;
    Tile& tile = At_(game.pState->GetWorldMap(), 4, 4);
    tile.SetElevation(-100);
    REQUIRE(tile.IsWater());
    game.pState->GetTileEffects().AddImprovementWithEffects(tile, "KelpFarm");
    game.pState->GetTileEffects().AddImprovementWithEffects(tile, "Road");

    Unit& warhead = game.MakeWarhead(4, 4, /*reactorLevels=*/1);
    REQUIRE(ApplyDetonation(*game.pState, warhead));

    CHECK(tile.IsLand());
    CHECK_FALSE(tile.HasImprovement("KelpFarm"));
    CHECK(tile.HasImprovement("Road"));
}

TEST_CASE("A design with no detonation list cannot detonate", "[unit][elevation][detonate]")
{
    DetonateGame_ game;
    Tile* pTile = game.pState->GetWorldMap().GetTile(2, 2);
    REQUIRE(pTile);

    const UnitComponentConfig_t* pChassis = game.fixtures.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    game.designs.emplace_back(
        slots, std::unordered_map<std::string, const UnitComponentConfig_t*>{
                   {"chassis", pChassis}});
    Unit& plain = game.pFaction->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), game.designs.back(),
        game.pState->GetWorldMap().GetUnitPositions(), *pTile);
    const int before = pTile->GetElevation();

    CHECK_FALSE(UnitCanDetonate(plain));
    CHECK_FALSE(ApplyDetonation(*game.pState, plain));
    CHECK(pTile->GetElevation() == before);
}
