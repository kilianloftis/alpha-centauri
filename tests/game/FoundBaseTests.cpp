#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/ElevationChange.h"
#include "game/map/ImprovementIds.h"
#include "game/map/SurfaceOccupancy.h"
#include "game/units/FoundBaseRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void RebuildTerritory_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

std::vector<const BaseManager*> AllBases_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    return bases;
}

size_t CountUnits_(Faction& rFaction)
{
    return static_cast<size_t>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

struct FoundBaseGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    FoundBaseGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules, fixtures.dataContext.interactionGrids, actest::k_TestRngSeed);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, pState->GetWorldMap(), fixtures.settings,
            actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, pState->GetWorldMap(), fixtures.settings,
            actest::k_TestFactionSeed);

        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds,
                   BaseManager* pHomeBase = nullptr)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        fixtures.designs.emplace_back(slots, assigned);

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile,
            pHomeBase);
    }
};

} // namespace

TEST_CASE("Founding is illegal within 2 tiles of an existing base", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    const auto bases = AllBases_(fixture);
    CHECK_FALSE(CanFoundBaseAt(fixture.At(4, 4), faction.GetFactionId(), fixture.map, bases)); // 0
    CHECK_FALSE(CanFoundBaseAt(fixture.At(5, 4), faction.GetFactionId(), fixture.map, bases)); // 1
    CHECK_FALSE(CanFoundBaseAt(fixture.At(6, 4), faction.GetFactionId(), fixture.map, bases)); // 2
    CHECK(CanFoundBaseAt(fixture.At(7, 4), faction.GetFactionId(), fixture.map, bases));       // 3
}

TEST_CASE("Founding is illegal in another faction's territory", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 4, 4);
    RebuildTerritory_(fixture);

    REQUIRE(fixture.map.GetTerritory().GetOwner(5, 4) == owner.GetFactionId());

    // Far enough from the base for spacing, but still in owner's disk.
    Tile& inOwnerTerritory = fixture.At(4, 0);
    REQUIRE(ChebyshevDistance(fixture.At(4, 4), inOwnerTerritory, fixture.map.GetWidth()) >= 3);
    REQUIRE(fixture.map.GetTerritory().GetOwner(inOwnerTerritory) == owner.GetFactionId());

    CHECK_FALSE(CanFoundBaseAt(inOwnerTerritory, other.GetFactionId(), fixture.map, AllBases_(fixture)));
    CHECK(CanFoundBaseAt(inOwnerTerritory, owner.GetFactionId(), fixture.map, AllBases_(fixture)));
}

TEST_CASE("TryFoundBase creates a base; SingleUse expends the colony pod", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    REQUIRE(pod.GetFlag(RuleFlagId_t::FoundBase));
    REQUIRE(pod.GetFlag(RuleFlagId_t::SingleUse));
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    REQUIRE(CountUnits_(*game.pPlayer) == 1);

    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    CHECK(pNew->GetTile().GetX() == 7);
    CHECK(pNew->GetTile().GetY() == 4);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 0);
    CHECK(game.pState->FindBaseAt(7, 4) == pNew);
    // Colony pod's StartingMinerals Add 10 lands in the new base's production stockpile.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 10);
    CHECK(game.pPlayer->GetHeadquarters() == &home);
    CHECK(home.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK_FALSE(pNew->GetBuildingManager().HasBuilding("Headquarters"));
}

TEST_CASE("The first founded base is Headquarters and keeps all energy", "[unit][found-base][hq]")
{
    FoundBaseGame_ game;
    REQUIRE(game.pPlayer->GetHeadquarters() == nullptr);

    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    CHECK(game.pPlayer->GetHeadquarters() == &home);
    CHECK(home.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(home.GetBuildingUpkeep() == 0);

    // HQ short-circuits inefficiency: allocatable energy equals pre-inefficiency production.
    CHECK(home.GetEconProduction() + home.GetLabsProduction() + home.GetPsychProduction()
          == home.GetEnergyProduction());
}

TEST_CASE("TryFoundBase stacks founding-unit and AllOwnerBases StartingMinerals",
          "[unit][found-base][starting-minerals]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    home.GetBuildingManager().AddBuilding("founding_minerals_project");

    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    // Pod 10 + project AllOwnerBases +5.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 15);
}

TEST_CASE("Support -2 cancels the colony pod's 10 free founding minerals",
          "[unit][found-base][starting-minerals][support][rating]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    game.pPlayer->GetSocialEngineering().SetActivePolicy(
        game.fixtures.socialPolicies().Get("low_support_policy"));
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Support) == -2);

    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    // Pod +10 offset by Support ≤ -2 StartingMinerals -10.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Support -2 still allows project StartingMinerals above the cancelled 10",
          "[unit][found-base][starting-minerals][support][rating]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    home.GetBuildingManager().AddBuilding("founding_minerals_project");
    game.pPlayer->GetSocialEngineering().SetActivePolicy(
        game.fixtures.socialPolicies().Get("low_support_policy"));

    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    // Pod 10 + project 5 + Support -10 → 5.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 5);
}

TEST_CASE("Founding minerals above the retool threshold may switch freely",
          "[unit][found-base][starting-minerals][retool]")
{
    // Null turn original ⇒ free queue/switch — covered in ProductionCostTests. Here assert
    // the founding path credits a bank that survives queue/switch (stock fixtures have zero
    // mineral_cost and would complete).
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& pod =
        game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod_rich"}, &home);

    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    REQUIRE(pNew->GetProduction().GetMineralStockpile() == 40);

    const BuildingConfig_t* pFacilityA =
        game.fixtures.dataContext.buildingRegistry->Find("test_facility_a");
    const BuildingConfig_t* pFacilityB =
        game.fixtures.dataContext.buildingRegistry->Find("test_facility_b");
    REQUIRE(pFacilityA);
    REQUIRE(pFacilityB);

    ProductionManager& rProd = pNew->GetProduction();
    rProd.SetProduction(pFacilityA, pNew->GetBaseEffects());
    CHECK(rProd.GetMineralStockpile() == 40);
    rProd.SetProduction(pFacilityB, pNew->GetBaseEffects());
    CHECK(rProd.GetMineralStockpile() == 40);
}

TEST_CASE("TryFoundBase without SingleUse leaves the unit alive", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_found_base_only"}, &home);
    REQUIRE(pod.GetFlag(RuleFlagId_t::FoundBase));
    REQUIRE_FALSE(pod.GetFlag(RuleFlagId_t::SingleUse));

    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 1);
}

TEST_CASE("TryFoundBase fails without FoundBase or on an illegal tile", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);

    Unit& scout = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        scout, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 1);

    Unit& tooClose = game.MakeUnit(*game.pPlayer, 5, 4, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        tooClose, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 2);

    game.MakeBase(*game.pAi, 0, 0);
    Tile* pAiBaseTile = game.pState->GetWorldMap().GetTile(0, 0);
    Tile* pForeign = game.pState->GetWorldMap().GetTile(0, 3);
    REQUIRE(pAiBaseTile);
    REQUIRE(pForeign);
    REQUIRE(ChebyshevDistance(*pAiBaseTile, *pForeign, game.pState->GetWorldMap().GetWidth()) >= 3);
    REQUIRE(game.pState->GetWorldMap().GetTerritory().GetOwner(*pForeign) == game.pAi->GetFactionId());

    Unit& inForeign = game.MakeUnit(*game.pPlayer, 0, 3, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        inForeign, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
}

void CrossElevation_(GameState& rState, Tile& rTile, int deltaMeters)
{
    const ElevationRulesConfig_t& rRules = rTile.MapRules();
    REQUIRE(ApplyElevationDelta(rTile, rState.GetWorldMap(), deltaMeters, rRules,
                                rRules.minElevationMeters, rRules.maxElevationMeters,
                                &rState.GetTileEffects(), &rState));
}

TEST_CASE("A land colony pod cannot found on water; a sea pod can and may occupy water",
          "[unit][found-base][surface]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Tile& water = *game.pState->GetWorldMap().GetTile(7, 4);
    water.SetElevation(-100);
    REQUIRE(water.IsWater());

    Unit& landPod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    CHECK(landPod.GetDomain() == UnitDomain_t::Land);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        landPod, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);

    Unit& seaOnLand = game.MakeUnit(*game.pPlayer, 1, 4, {"test_sea_chassis", "test_colony_pod"}, &home);
    BaseManager* pLandFounded = game.pState->GetUnitOrderExecutor().TryFoundBase(
        seaOnLand, *game.pState, game.fixtures.dataContext);
    REQUIRE(pLandFounded);
    CHECK_FALSE(pLandFounded->MayOccupyWater());

    Unit& seaPod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_sea_chassis", "test_colony_pod"}, &home);
    BaseManager* pSea = game.pState->GetUnitOrderExecutor().TryFoundBase(
        seaPod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pSea);
    CHECK(pSea->GetTile().IsWater());
    CHECK(pSea->MayOccupyWater());
    CHECK(pSea->GetTile().HasImprovement(ImprovementIds::k_Base));
}

TEST_CASE("A surface flip removes improvements whose domain no longer matches",
          "[unit][elevation][surface]")
{
    FoundBaseGame_ game;
    Tile& origin = *game.pState->GetWorldMap().GetTile(4, 4);
    Tile& neighbor = *game.pState->GetWorldMap().GetTile(4, 5);
    REQUIRE(origin.IsLand());
    game.pState->GetTileEffects().AddImprovementWithEffects(origin, "Farm");
    game.pState->GetTileEffects().AddImprovementWithEffects(origin, "Road");
    game.pState->GetTileEffects().AddImprovementWithEffects(neighbor, "Farm");
    game.pState->GetTileEffects().AddImprovementWithEffects(neighbor, "Road");

    // 100 - 2000 = -1900. The neighbor is pulled down to stay within 1500m, onto water.
    CrossElevation_(*game.pState, origin, -2000);
    CHECK(origin.IsWater());
    CHECK(neighbor.IsWater());
    CHECK_FALSE(origin.HasImprovement("Farm"));
    CHECK(origin.HasImprovement("Road"));
    CHECK_FALSE(neighbor.HasImprovement("Farm"));
    CHECK(neighbor.HasImprovement("Road"));

    Tile& sea = *game.pState->GetWorldMap().GetTile(1, 1);
    sea.SetElevation(-100);
    game.pState->GetTileEffects().AddImprovementWithEffects(sea, "KelpFarm");
    game.pState->GetTileEffects().AddImprovementWithEffects(sea, "Road");
    CrossElevation_(*game.pState, sea, 500);
    CHECK(sea.IsLand());
    CHECK_FALSE(sea.HasImprovement("KelpFarm"));
    CHECK(sea.HasImprovement("Road"));
}

TEST_CASE("A land base is razed when its tile becomes water unless it may occupy water",
          "[unit][found-base][elevation][surface]")
{
    FoundBaseGame_ game;
    BaseManager& land = game.MakeBase(*game.pPlayer, 4, 4);
    game.pState->GetTileEffects().AddImprovementWithEffects(land.GetTile(), "Farm");
    CrossElevation_(*game.pState, land.GetTile(), -500);
    CHECK(land.GetTile().IsWater());
    CHECK(land.IsRazed());
    CHECK_FALSE(land.GetTile().HasImprovement(ImprovementIds::k_Base));
    CHECK_FALSE(land.GetTile().HasImprovement("Farm"));
    CHECK(game.pState->FindBaseAt(4, 4) == nullptr);

    BaseManager& domed = game.MakeBase(*game.pPlayer, 4, 7);
    domed.GetBuildingManager().AddBuilding(k_PressureDomeBuildingId);
    game.pState->GetTileEffects().AddImprovementWithEffects(domed.GetTile(), "Farm");
    CrossElevation_(*game.pState, domed.GetTile(), -500);
    CHECK(domed.GetTile().IsWater());
    CHECK_FALSE(domed.IsRazed());
    CHECK(domed.GetTile().HasImprovement(ImprovementIds::k_Base));
    CHECK_FALSE(domed.GetTile().HasImprovement("Farm"));

    Tile& water = *game.pState->GetWorldMap().GetTile(7, 1);
    water.SetElevation(-100);
    Unit& seaPod = game.MakeUnit(*game.pPlayer, 7, 1, {"test_sea_chassis", "test_colony_pod"});
    BaseManager* pSea = game.pState->GetUnitOrderExecutor().TryFoundBase(
        seaPod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pSea);
    REQUIRE(pSea->MayOccupyWater());
    CrossElevation_(*game.pState, pSea->GetTile(), 500);
    CHECK(pSea->GetTile().IsLand());
    CHECK_FALSE(pSea->IsRazed());
    CrossElevation_(*game.pState, pSea->GetTile(), -500);
    CHECK(pSea->GetTile().IsWater());
    CHECK_FALSE(pSea->IsRazed());
    CHECK(pSea->MayOccupyWater());
    CHECK(pSea->GetTile().HasImprovement(ImprovementIds::k_Base));
}
