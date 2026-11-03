#include "GameFixtures.h"
#include "game/population/PopulationManager.h"

#include "game/CurrentGameState.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionManager.h"
#include "game/terraform/ElevationChange.h"
#include "game/map/ImprovementIds.h"
#include "game/terraform/SurfaceOccupancy.h"
#include "game/terraform/TerraformRules.h"
#include "game/base/BaseMutations.h"
#include "game/base/BaseRules.h"
#include "game/base/FoundBaseRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/buildings/BuildingRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
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
    for (const Faction& rFaction : rFixture.state.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

std::vector<const BaseManager*> AllBases_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : rFixture.state.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
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
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    FoundBaseGame_()
    {
        FillLand(fixtures.map);

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings,
            fixtures.state.GetUnitIdAllocator(),
            actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings,
            fixtures.state.GetUnitIdAllocator(),
            actest::k_TestFactionSeed);

        pPlayer = &fixtures.state.AddFaction(std::move(pFactionA));
        pAi = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    // The caller's founding path: CanFoundBase, then the mutation. nullptr when it may not.
    BaseManager* FoundBase(Unit& rFounder)
    {
        return CanFoundBase(rFounder) ? &ac::FoundBase(fixtures.state, rFounder) : nullptr;
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects(), std::nullopt);
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

        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), fixtures.designs.back(),
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, ac::UnitOrigin::Gift(pHomeBase));
    }
};

} // namespace

TEST_CASE("Founding is illegal within 2 tiles of an existing base", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 8, 8);
    RebuildTerritory_(fixture);

    const auto bases = AllBases_(fixture);
    CHECK_FALSE(CanFoundBaseAt(fixture.At(8, 8), faction.GetFactionId(), bases)); // 0
    CHECK_FALSE(CanFoundBaseAt(fixture.At(9, 9), faction.GetFactionId(), bases)); // 1
    CHECK_FALSE(CanFoundBaseAt(fixture.At(10, 10), faction.GetFactionId(), bases)); // 2
    CHECK(CanFoundBaseAt(fixture.At(11, 11), faction.GetFactionId(), bases));       // 3
}

TEST_CASE("Founding is illegal in another faction's territory", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 8, 8);
    RebuildTerritory_(fixture);

    REQUIRE(fixture.map.GetTerritory().GetOwner(9, 9) == owner.GetFactionId());

    // Far enough from the base for spacing, but still in owner's disk.
    Tile& inOwnerTerritory = fixture.At(12, 4);
    REQUIRE(ChebyshevDistance(fixture.At(8, 8), inOwnerTerritory, fixture.map.GetWidth()) >= 3);
    REQUIRE(fixture.map.GetTerritory().GetOwner(inOwnerTerritory) == owner.GetFactionId());

    CHECK_FALSE(CanFoundBaseAt(inOwnerTerritory, other.GetFactionId(), AllBases_(fixture)));
    CHECK(CanFoundBaseAt(inOwnerTerritory, owner.GetFactionId(), AllBases_(fixture)));
}

TEST_CASE("FoundBase creates a base; SingleUse expends the colony pod", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    home.GetBuildingManager().AddBuilding("Headquarters");
    Unit& pod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"}, &home);
    REQUIRE(ResolveFlag(pod, RuleFlagId_t::FoundBase));
    REQUIRE(ResolveFlag(pod, RuleFlagId_t::SingleUse));
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    REQUIRE(CountUnits_(*game.pPlayer) == 1);

    BaseManager* pNew = game.FoundBase(pod);
    REQUIRE(pNew);
    CHECK(pNew->GetTile().GetX() == 11);
    CHECK(pNew->GetTile().GetY() == 11);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 0);
    CHECK(game.fixtures.state.FindBaseAt(11, 11) == pNew);
    // Colony pod's StartingMinerals Add 10 lands in the new base's production stockpile.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 10);
    CHECK(game.pPlayer->GetHeadquarters() == &home);
    CHECK(home.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK_FALSE(pNew->GetBuildingManager().HasBuilding("Headquarters"));
}

TEST_CASE("A Headquarters base keeps all energy", "[unit][found-base][hq]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    home.GetBuildingManager().AddBuilding("Headquarters");
    CHECK(game.pPlayer->GetHeadquarters() == &home);
    CHECK(home.GetBuildingUpkeep() == 0);

    // HQ short-circuits inefficiency: allocatable energy equals pre-inefficiency production.
    CHECK(home.GetEconProduction() + home.GetLabsProduction() + home.GetPsychProduction()
          == home.GetEnergyProduction());
}

TEST_CASE("FoundBase stacks founding-unit and AllOwnerBases StartingMinerals",
          "[unit][found-base][starting-minerals]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    home.GetBuildingManager().AddBuilding("founding_minerals_project");

    Unit& pod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.FoundBase(pod);
    REQUIRE(pNew);
    // Pod 10 + project AllOwnerBases +5.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 15);
}

TEST_CASE("Support -2 cancels the colony pod's 10 free founding minerals",
          "[unit][found-base][starting-minerals][support][rating]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    game.pPlayer->GetSocialEngineering().SetActivePolicy(
        game.fixtures.socialPolicies().Get("low_support_policy"));
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Support) == -2);

    Unit& pod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.FoundBase(pod);
    REQUIRE(pNew);
    // Pod +10 offset by Support ≤ -2 StartingMinerals -10.
    CHECK(pNew->GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Support -2 still allows project StartingMinerals above the cancelled 10",
          "[unit][found-base][starting-minerals][support][rating]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    home.GetBuildingManager().AddBuilding("founding_minerals_project");
    game.pPlayer->GetSocialEngineering().SetActivePolicy(
        game.fixtures.socialPolicies().Get("low_support_policy"));

    Unit& pod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"}, &home);
    BaseManager* pNew = game.FoundBase(pod);
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    Unit& pod =
        game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod_rich"}, &home);

    BaseManager* pNew = game.FoundBase(pod);
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

TEST_CASE("FoundBase without SingleUse leaves the unit alive", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    Unit& pod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_found_base_only"}, &home);
    REQUIRE(ResolveFlag(pod, RuleFlagId_t::FoundBase));
    REQUIRE_FALSE(ResolveFlag(pod, RuleFlagId_t::SingleUse));

    BaseManager* pNew = game.FoundBase(pod);
    REQUIRE(pNew);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 1);
}

TEST_CASE("FoundBase fails without FoundBase or on an illegal tile", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);

    Unit& scout = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis"}, &home);
    CHECK_FALSE(game.FoundBase(scout));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 1);

    Unit& tooClose = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.FoundBase(tooClose));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 2);

    game.MakeBase(*game.pAi, 8, 0);
    Tile* pAiBaseTile = game.fixtures.state.GetWorldMap().GetTile(8, 0);
    Tile* pForeign = game.fixtures.state.GetWorldMap().GetTile(5, 3);
    REQUIRE(pAiBaseTile);
    REQUIRE(pForeign);
    REQUIRE(ChebyshevDistance(*pAiBaseTile, *pForeign, game.fixtures.state.GetWorldMap().GetWidth()) >= 3);
    REQUIRE(game.fixtures.state.GetWorldMap().GetTerritory().GetOwner(*pForeign) == game.pAi->GetFactionId());

    Unit& inForeign = game.MakeUnit(*game.pPlayer, 5, 3, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.FoundBase(inForeign));
    CHECK(game.pPlayer->GetBaseCount() == 1);
}

void CrossElevation_(GameState& rState, Tile& rTile, int deltaMeters)
{
    const MapRulesConfig_t& rRules = rTile.MapRules();
    REQUIRE(ApplyElevationDelta(rTile, deltaMeters, rRules, rRules.minElevationMeters,
                                rRules.maxElevationMeters, rState));
}

TEST_CASE("A land colony pod cannot found on water; a sea pod can and may occupy water",
          "[unit][found-base][surface]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 8);
    Tile& water = *game.fixtures.state.GetWorldMap().GetTile(11, 11);
    water.SetElevation(-100);
    REQUIRE(water.IsWater());

    Unit& landPod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"}, &home);
    CHECK(landPod.GetDomain() == UnitDomain_t::Land);
    CHECK_FALSE(game.FoundBase(landPod));
    CHECK(game.pPlayer->GetBaseCount() == 1);

    Unit& seaOnLand = game.MakeUnit(*game.pPlayer, 5, 5, {"test_sea_chassis", "test_colony_pod"}, &home);
    BaseManager* pLandFounded = game.FoundBase(seaOnLand);
    REQUIRE(pLandFounded);
    CHECK_FALSE(pLandFounded->MayOccupyWater());

    Unit& seaPod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_sea_chassis", "test_colony_pod"}, &home);
    BaseManager* pSea = game.FoundBase(seaPod);
    REQUIRE(pSea);
    CHECK(pSea->GetTile().IsWater());
    CHECK(pSea->MayOccupyWater());
    CHECK(pSea->GetTile().HasImprovement(ImprovementIds::k_Base));
}

TEST_CASE("A surface flip removes improvements whose domain no longer matches",
          "[unit][elevation][surface]")
{
    FoundBaseGame_ game;
    Tile& origin = *game.fixtures.state.GetWorldMap().GetTile(8, 8);
    Tile& neighbor = *game.fixtures.state.GetWorldMap().GetTile(7, 9);
    REQUIRE(origin.IsLand());
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(origin, "Farm");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(origin, "Road");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(neighbor, "Farm");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(neighbor, "Road");

    // 100 - 2000 = -1900. The neighbor is pulled down to stay within 1500m, onto water.
    CrossElevation_(game.fixtures.state, origin, -2000);
    CHECK(origin.IsWater());
    CHECK(neighbor.IsWater());
    CHECK_FALSE(origin.HasImprovement("Farm"));
    CHECK(origin.HasImprovement("Road"));
    CHECK_FALSE(neighbor.HasImprovement("Farm"));
    CHECK(neighbor.HasImprovement("Road"));

    Tile& sea = *game.fixtures.state.GetWorldMap().GetTile(8, 2);
    sea.SetElevation(-100);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(sea, "KelpFarm");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(sea, "Road");
    CrossElevation_(game.fixtures.state, sea, 500);
    CHECK(sea.IsLand());
    CHECK_FALSE(sea.HasImprovement("KelpFarm"));
    CHECK(sea.HasImprovement("Road"));
}

namespace
{

bool HasUnit_(Faction& rFaction, UnitId_t unitId)
{
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return true;
        }
    }
    return false;
}

void GrowTo_(BaseManager& rBase, int size)
{
    while (rBase.GetPopulation().GetSize() < size)
    {
        rBase.GetPopulation().AddPop("Worker");
    }
}

} // namespace

TEST_CASE("A domed base and a sea base keep their tiles across a flip",
          "[unit][found-base][elevation][surface]")
{
    FoundBaseGame_ game;
    game.MakeBase(*game.pPlayer, 14, 14);
    BaseManager& domed = game.MakeBase(*game.pPlayer, 5, 11);
    domed.GetBuildingManager().AddBuilding(k_PressureDomeBuildingId);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(domed.GetTile(), "Farm");
    const int domedSize = domed.GetPopulation().GetSize();
    CrossElevation_(game.fixtures.state, domed.GetTile(), -500);
    CHECK(domed.GetTile().IsWater());
    CHECK_FALSE(domed.IsRazed());
    CHECK(domed.GetPopulation().GetSize() == domedSize);
    CHECK(domed.GetTile().HasImprovement(ImprovementIds::k_Base));
    CHECK_FALSE(domed.GetTile().HasImprovement("Farm"));

    Tile& water = *game.fixtures.state.GetWorldMap().GetTile(14, 8);
    water.SetElevation(-100);
    Unit& seaPod = game.MakeUnit(*game.pPlayer, 14, 8, {"test_sea_chassis", "test_colony_pod"});
    BaseManager* pSea = game.FoundBase(seaPod);
    REQUIRE(pSea);
    REQUIRE(pSea->MayOccupyWater());
    CrossElevation_(game.fixtures.state, pSea->GetTile(), 500);
    CHECK(pSea->GetTile().IsLand());
    CHECK_FALSE(pSea->IsRazed());
    CrossElevation_(game.fixtures.state, pSea->GetTile(), -500);
    CHECK(pSea->GetTile().IsWater());
    CHECK_FALSE(pSea->IsRazed());
    CHECK(pSea->GetTile().HasImprovement(ImprovementIds::k_Base));
}

TEST_CASE("A flooded base loses half its population and gains a Pressure Dome",
          "[unit][found-base][elevation][surface]")
{
    FoundBaseGame_ game;
    game.MakeBase(*game.pPlayer, 14, 14);
    BaseManager& flooded = game.MakeBase(*game.pPlayer, 8, 8);
    actest::SetMaxBaseSize(*game.fixtures.dataContext.growthConfig, 20);
    GrowTo_(flooded, 10);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(flooded.GetTile(), "Farm");
    CrossElevation_(game.fixtures.state, flooded.GetTile(), -500);

    // Half of 10 beats every roll (2-4) and stays under the cap of 10.
    CHECK_FALSE(flooded.IsRazed());
    CHECK(flooded.GetPopulation().GetSize() == 5);
    CHECK(flooded.GetBuildingManager().HasBuilding(k_PressureDomeBuildingId));
    CHECK(flooded.GetTile().HasImprovement(ImprovementIds::k_Base));
    CHECK_FALSE(flooded.GetTile().HasImprovement("Farm"));
}

TEST_CASE("A flooded base left with no population is destroyed, unless it is the last one",
          "[unit][found-base][elevation][surface]")
{
    FoundBaseGame_ game;
    BaseManager& last = game.MakeBase(*game.pAi, 4, 4);
    REQUIRE(last.GetPopulation().GetSize() == 1);
    CrossElevation_(game.fixtures.state, last.GetTile(), -500);
    CHECK_FALSE(last.IsRazed());
    CHECK(last.GetPopulation().GetSize() == 1);
    CHECK(last.GetBuildingManager().HasBuilding(k_PressureDomeBuildingId));

    game.MakeBase(*game.pPlayer, 14, 14);
    BaseManager& small = game.MakeBase(*game.pPlayer, 8, 8);
    REQUIRE(small.GetPopulation().GetSize() == 1);
    CrossElevation_(game.fixtures.state, small.GetTile(), -500);
    CHECK(small.IsRazed());
    CHECK_FALSE(small.GetTile().HasImprovement(ImprovementIds::k_Base));
    CHECK(game.fixtures.state.FindBaseAt(8, 8) == nullptr);
}

TEST_CASE("A flip kills units on the wrong surface unless a standing base shelters them",
          "[unit][found-base][elevation][surface]")
{
    FoundBaseGame_ game;
    Unit& exposed = game.MakeUnit(*game.pPlayer, 10, 4, {"test_chassis"});
    const UnitId_t exposedId = exposed.GetUnitId();
    CrossElevation_(game.fixtures.state, *game.fixtures.state.GetWorldMap().GetTile(10, 4), -500);
    CHECK_FALSE(HasUnit_(*game.pPlayer, exposedId));

    game.MakeBase(*game.pPlayer, 14, 14);
    BaseManager& flooded = game.MakeBase(*game.pPlayer, 8, 8);
    actest::SetMaxBaseSize(*game.fixtures.dataContext.growthConfig, 20);
    GrowTo_(flooded, 10);
    Unit& garrison = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis"});
    const UnitId_t garrisonId = garrison.GetUnitId();
    CrossElevation_(game.fixtures.state, flooded.GetTile(), -500);
    REQUIRE_FALSE(flooded.IsRazed());
    CHECK(HasUnit_(*game.pPlayer, garrisonId));
}

TEST_CASE("Only a sea colony founds on water, and its base keeps the water tile",
          "[unit][found-base]")
{
    FoundBaseGame_ game;
    game.fixtures.state.GetWorldMap().GetTile(11, 11)->SetElevation(-100);
    Unit& landPod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_chassis", "test_colony_pod"});
    Unit& seaPod = game.MakeUnit(*game.pPlayer, 11, 11, {"test_sea_chassis", "test_colony_pod"});
    Unit& rover = game.MakeUnit(*game.pPlayer, 12, 12, {"test_chassis"});

    CHECK_FALSE(CanFoundBase(landPod));
    CHECK(CanFoundBase(seaPod));
    CHECK_FALSE(CanFoundBase(rover));
}

TEST_CASE("A flip to water names what it removes and which base floods",
          "[unit][found-base][surface]")
{
    FoundBaseGame_ game;
    BaseManager& land = game.MakeBase(*game.pPlayer, 8, 8);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(land.GetTile(), "Farm");
    BaseManager& domed = game.MakeBase(*game.pPlayer, 5, 11);
    domed.GetBuildingManager().AddBuilding(k_PressureDomeBuildingId);

    const SurfaceFlipConsequences_t flooded =
        SurfaceFlipConsequences(land.GetTile(), /*bNowWater=*/true, &land);
    REQUIRE(flooded.submerged.has_value());
    CHECK(flooded.submerged->size == land.GetPopulation().GetSize());
    CHECK_FALSE(flooded.submerged->bLastBase);
    CHECK(std::ranges::count(flooded.removedImprovements, std::string(ImprovementIds::k_Base))
          == 0);
    CHECK(std::ranges::count(flooded.removedImprovements, std::string("Farm")) == 1);
    CHECK_FALSE(land.IsRazed());

    const SurfaceFlipConsequences_t spared =
        SurfaceFlipConsequences(domed.GetTile(), /*bNowWater=*/true, &domed);
    CHECK_FALSE(spared.submerged.has_value());
}

TEST_CASE("Submergence loses clamp(size / 2, roll, cap) pop", "[surface]")
{
    SubmergedBase_t base;
    base.rollMin = 2;
    base.rollMax = 4;
    base.lossCap = 10;

    base.size = 7; // half 3
    CHECK(ResolveSubmergence(base, 2).popLoss == 3);
    CHECK(ResolveSubmergence(base, 4).popLoss == 4);
    base.size = 30; // half 15, capped
    CHECK(ResolveSubmergence(base, 2).popLoss == 10);

    base.size = 3;
    CHECK(ResolveSubmergence(base, 4).bDestroyed);
    base.bLastBase = true;
    const SubmergenceOutcome_t last = ResolveSubmergence(base, 4);
    CHECK_FALSE(last.bDestroyed);
    CHECK(last.popLoss == 2);
}

TEST_CASE("Founding a base grants no headquarters", "[found-base][hq]")
{
    FoundBaseGame_ game;
    REQUIRE(game.pPlayer->GetHeadquarters() == nullptr);

    const BaseManager& rBase = game.MakeBase(*game.pPlayer, 8, 8);

    CHECK_FALSE(rBase.IsHeadquarters());
    CHECK(game.pPlayer->GetHeadquarters() == nullptr);
}
