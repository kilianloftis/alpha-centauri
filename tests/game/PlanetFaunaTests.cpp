#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/natives/NativeUnitConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "game/natives/PlanetFaunaMutations.h"
#include "game/natives/PlanetFaunaRules.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/units/Military.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitManager.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void LoadFullNatives_(FactionFixture& rFixture)
{
    rFixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    rFixture.dataContext.nativeUnitRegistry->Load(FixturePath("native_units_full.json"));
}

const PlanetFaunaCalculator& Calc_(const FactionFixture& rFixture)
{
    return *rFixture.dataContext.planetFaunaCalculator;
}

void PlantFungus_(FactionFixture& rFixture, Tile& rTile)
{
    rTile.AddTerrainFeature(rFixture.state.GetTileEffects().GetImprovements().Get("Fungus"));
}

} // namespace

TEST_CASE("LifecycleFloor skips Great Boil", "[native][fauna][lifecycle]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeLifeConfig->planetLifecycleTurnThresholds =
        {45, 90, 170, 250, 0, 330};
    const PlanetFaunaCalculator& rCalc = Calc_(fixture);

    CHECK(rCalc.LifecycleFloor(0) == 0);
    CHECK(rCalc.LifecycleFloor(44) == 0);
    CHECK(rCalc.LifecycleFloor(45) == 1);
    CHECK(rCalc.LifecycleFloor(89) == 1);
    CHECK(rCalc.LifecycleFloor(90) == 2);
    CHECK(rCalc.LifecycleFloor(169) == 2);
    CHECK(rCalc.LifecycleFloor(170) == 3);
    CHECK(rCalc.LifecycleFloor(249) == 3);
    CHECK(rCalc.LifecycleFloor(250) == 4);
    CHECK(rCalc.LifecycleFloor(329) == 4);
    CHECK(rCalc.LifecycleFloor(330) == 6);
}

TEST_CASE("Wild native lifecycle rises with turn; Locusts get +1", "[native][fauna][lifecycle]")
{
    FactionFixture fixture;
    LoadFullNatives_(fixture);
    FillLand(fixture.map);
    Faction& rPlanet = AddNativeLifeFaction(fixture);
    GameStateScope scope(fixture.state);

    AdvanceToYear(fixture.state, fixture.state.GetMissionYear());
    Unit& rWorm = rPlanet.GetUnitManager().CreateUnit(
        fixture.state.AllocateUnitId(), *rPlanet.GetMilitary().GetDesign("Mind_Worm"),
        fixture.map.GetUnitPositions(), fixture.At(4, 4), UnitOrigin::Gift(nullptr));
    Unit& rLocust = rPlanet.GetUnitManager().CreateUnit(
        fixture.state.AllocateUnitId(), *rPlanet.GetMilitary().GetDesign("Locusts_of_Chiron"),
        fixture.map.GetUnitPositions(), fixture.At(5, 5), UnitOrigin::Gift(nullptr));

    const MoraleCalculator& rMorale = *fixture.dataContext.moraleCalculator;
    EffectContext_t ctx;
    ctx.pUnit = &rWorm;
    CHECK(EffectiveMoraleLevel(rWorm, ctx, rMorale) == 0);

    ctx.pUnit = &rLocust;
    CHECK(EffectiveMoraleLevel(rLocust, ctx, rMorale) == 1);

    while (fixture.state.GetTurnNumber() < 45)
    {
        fixture.state.IncrementMissionYear();
    }
    ctx.pUnit = &rWorm;
    CHECK(EffectiveMoraleLevel(rWorm, ctx, rMorale) == 1);
    ctx.pUnit = &rLocust;
    CHECK(EffectiveMoraleLevel(rLocust, ctx, rMorale) == 2);
}

TEST_CASE("Fungal Tower lifecycle is adjacent fungus minus two", "[native][fauna][lifecycle]")
{
    FactionFixture fixture;
    LoadFullNatives_(fixture);
    FillLand(fixture.map);
    Faction& rPlanet = AddNativeLifeFaction(fixture);
    GameStateScope scope(fixture.state);

    Tile& rCenter = fixture.At(8, 8);
    PlantFungus_(fixture, fixture.At(7, 7));
    PlantFungus_(fixture, fixture.At(9, 7));
    PlantFungus_(fixture, fixture.At(7, 9));

    Unit& rTower = rPlanet.GetUnitManager().CreateUnit(
        fixture.state.AllocateUnitId(), *rPlanet.GetMilitary().GetDesign("Fungal_Tower"),
        fixture.map.GetUnitPositions(), rCenter, UnitOrigin::Gift(nullptr));

    const MoraleCalculator& rMorale = *fixture.dataContext.moraleCalculator;
    EffectContext_t ctx;
    ctx.pUnit = &rTower;
    CHECK(EffectiveMoraleLevel(rTower, ctx, rMorale) == 1);
}

TEST_CASE("Planet fauna formulas use difficulty and native forms", "[native][fauna]")
{
    FactionFixture fixture;
    const PlanetFaunaCalculator& rCalc = Calc_(fixture);

    CHECK(rCalc.BoatPeriod(1) == 8);
    CHECK(PlanetFaunaEarlyLimit(20, rCalc, 1, 2));
    CHECK_FALSE(PlanetFaunaEarlyLimit(40, rCalc, 1, 2));
    CHECK(PlanetFaunaBoatCargo(40, rCalc, 1, 2, 1.0, true) == 1);
    CHECK(PlanetFaunaBoatCargo(40, rCalc, 1, 2, 1.0, false) == 40 / (4 * (4 * 7 - 2)) + 2);
    CHECK(PlanetFaunaBoatCargo(40, rCalc, 1, 2, 2.0, false) == 80 / (4 * (4 * 7 - 2)) + 2);
    CHECK(rCalc.AmbushAttempts(1) == 4);
    CHECK(PlanetFaunaSwarmYearReached(40, rCalc, 1));
    CHECK_FALSE(PlanetFaunaSwarmYearReached(39, rCalc, 1));
}

TEST_CASE("Boat spawn places an Isle with embarked land cargo on a fungus tile",
          "[native][fauna]")
{
    FactionFixture fixture;
    LoadFullNatives_(fixture);
    // Sea Lurk is also boat_hull; keep only Isle so cargo capacity is in play.
    std::vector<NativeUnitConfig_t> nativeConfigs =
        fixture.dataContext.nativeUnitRegistry->GetAll();
    for (NativeUnitConfig_t& rConfig : nativeConfigs)
    {
        if (rConfig.id == "Sea_Lurk" && rConfig.planet.has_value())
        {
            rConfig.planet->faunaWeight = 0;
        }
    }
    fixture.dataContext.nativeUnitRegistry->Assign(std::move(nativeConfigs));

    FillLand(fixture.map);
    Faction& rPlanet = AddNativeLifeFaction(fixture);
    GameStateScope scope(fixture.state);

    for (auto& pTile : fixture.map.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        pTile->SetElevation(-100);
        PlantFungus_(fixture, *pTile);
    }

    // Period 8 for fauna_forms 1; turn >= 45 so Isle IntrinsicXp cargo capacity is non-zero.
    while (fixture.state.GetTurnNumber() < 45 || fixture.state.GetTurnNumber() % 8 != 0)
    {
        fixture.state.IncrementMissionYear();
    }

    std::mt19937 rng(1);
    RunPlanetFauna(fixture.state, rng);

    int isleCount = 0;
    int embarkedCargo = 0;
    for (const Unit& rUnit : rPlanet.GetUnitManager().Units())
    {
        if (rUnit.GetDesign().GetId() != "Isle_of_the_Deep")
        {
            continue;
        }
        ++isleCount;
        for (const Unit* pPassenger : rUnit.Transport().GetCargo())
        {
            REQUIRE(pPassenger);
            CHECK(pPassenger->Transport().GetCarrier() == &rUnit);
            CHECK(pPassenger->GetDomain() == UnitDomain_t::Land);
            ++embarkedCargo;
        }
    }
    REQUIRE(isleCount == 1);
    CHECK(embarkedCargo >= 1);
}

TEST_CASE("SuppressPlanetFauna resolves from active effects for owned and destroyed SPs",
          "[native][fauna]")
{
    FactionFixture fixture;
    FillLand(fixture.map);
    Faction& rFaction = fixture.MakeFaction();
    GameStateScope scope(fixture.state);
    BaseManager& rBase = MakeSessionBase(fixture, rFaction, 8, 8);

    CHECK_FALSE(PlanetFaunaSuppressed());

    rBase.GetBuildingManager().AddBuilding("test_suppress_planet_fauna_sp");
    CHECK(PlanetFaunaSuppressed());
    CHECK(ResolveFlag(rFaction, RuleFlagId_t::SuppressPlanetFauna));

    rBase.GetBuildingManager().DestroyBuilding("test_suppress_planet_fauna_sp");
    CHECK(fixture.state.IsSecretProjectDestroyed("test_suppress_planet_fauna_sp"));
    CHECK(PlanetFaunaSuppressed());
    CHECK(ResolveFlag(rFaction, RuleFlagId_t::SuppressPlanetFauna));
}

TEST_CASE("SP WorldGlobal effects without destroyed_effects end when the project is razed",
          "[native][fauna]")
{
    FactionFixture fixture;
    FillLand(fixture.map);
    Faction& rFaction = fixture.MakeFaction();
    GameStateScope scope(fixture.state);
    BaseManager& rBase = MakeSessionBase(fixture, rFaction, 8, 8);

    rBase.GetBuildingManager().AddBuilding("test_suppress_fauna_owned_only_sp");
    CHECK(PlanetFaunaSuppressed());

    rBase.GetBuildingManager().DestroyBuilding("test_suppress_fauna_owned_only_sp");
    CHECK(fixture.state.IsSecretProjectDestroyed("test_suppress_fauna_owned_only_sp"));
    CHECK_FALSE(PlanetFaunaSuppressed());
}

TEST_CASE("Locusts stay locked until a faction has centauri_psi", "[native][fauna]")
{
    FactionFixture fixture;
    LoadFullNatives_(fixture);
    const NativeUnitConfig_t* pLocusts =
        fixture.dataContext.nativeUnitRegistry->Find("Locusts_of_Chiron");
    REQUIRE(pLocusts);
    REQUIRE(pLocusts->planet.has_value());
    REQUIRE_FALSE(pLocusts->planet->requiresAnyTech.empty());

    Faction& rHuman = fixture.MakeFaction();
    GameStateScope scope(fixture.state);
    CHECK_FALSE(AnyFactionHasTech(pLocusts->planet->requiresAnyTech));

    rHuman.GetResearch().AddDiscoveredTech("centauri_psi");
    CHECK(AnyFactionHasTech(pLocusts->planet->requiresAnyTech));
}
