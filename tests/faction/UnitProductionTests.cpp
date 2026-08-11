// Unit designs are IConstructable and appear in a base's production list; completing one
// spawns the unit on the base tile with home / produced-at set to that base.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

UnitSlotConfig_t MakeSlot_(std::string id, std::string componentType)
{
    UnitSlotConfig_t slot;
    slot.id = std::move(id);
    slot.displayName = slot.id;
    slot.componentType = std::move(componentType);
    slot.required = true;
    return slot;
}

std::unique_ptr<UnitDesign> MakeFixtureDesign_(FactionFixture& rFixtures,
                                               const std::vector<std::string>& rComponentIds)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : rComponentIds)
    {
        const UnitComponentConfig_t* pComponent = rFixtures.unitComponents.Find(rId);
        REQUIRE(pComponent != nullptr);
        UnitSlotConfig_t slot = MakeSlot_("slot_" + std::to_string(slotIndex++), pComponent->type);
        assigned[slot.id] = pComponent;
        slots.push_back(slot);
    }
    return std::make_unique<UnitDesign>(slots, assigned);
}

struct UnitProductionGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pFaction = nullptr;
    Faction* pOther = nullptr;

    UnitProductionGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, k_TestRngSeed);

        pFaction = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
        pOther = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed + 1));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(x, y),
            fixtures.dataContext, pState->GetTileEffects(),
            pState->GetSecretProjectAvailability());
        REQUIRE(pBase != nullptr);
        return *pBase;
    }

    BaseManager& MakeBase(int x, int y)
    {
        return MakeBase(*pFaction, x, y);
    }

    const UnitDesign& AddDesign(Faction& rFaction, const std::vector<std::string>& rComponentIds)
    {
        auto pDesign = MakeFixtureDesign_(fixtures, rComponentIds);
        const UnitDesign& rDesign = *pDesign;
        REQUIRE(rFaction.GetMilitary().AddDesign(std::move(pDesign)));
        return rDesign;
    }

    const UnitDesign& AddDesign(const std::vector<std::string>& rComponentIds)
    {
        return AddDesign(*pFaction, rComponentIds);
    }
};

bool ConstructableContains_(const std::vector<const IConstructable*>& rAvailable,
                            const IConstructable* pItem)
{
    return std::find(rAvailable.begin(), rAvailable.end(), pItem) != rAvailable.end();
}

} // namespace

TEST_CASE("All unit designs appear in the base constructable list", "[production][unit]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(2, 2);

    const UnitDesign& rScout = game.AddDesign({"test_chassis", "test_weapon", "test_armor"});
    const UnitDesign& rSlow = game.AddDesign({"test_slow_chassis", "test_weapon", "test_armor"});

    const std::vector<const IConstructable*> available = base.GetConstructable();
    CHECK(ConstructableContains_(available, &rScout));
    CHECK(ConstructableContains_(available, &rSlow));

    // Buildings still appear alongside designs.
    const BuildingConfig_t* pFacility = game.fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);
    CHECK(ConstructableContains_(available, pFacility));
}

TEST_CASE("Completing unit production places the unit on the base tile", "[production][unit]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign = game.AddDesign({"test_chassis", "test_weapon", "test_armor"});

    REQUIRE(game.pFaction->GetUnitManager().Units().empty());
    REQUIRE(game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile()).empty());

    base.GetProduction().SetProduction(&rDesign);
    REQUIRE(base.GetMineralCost() >= 1);
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    CHECK(base.ApplyProduction() == rDesign.GetId());
    CHECK_FALSE(base.GetProduction().HasProduction());

    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile());
    REQUIRE(onTile.size() == 1);
    Unit& rUnit = *onTile.front();
    CHECK(&rUnit.GetTile() == &base.GetTile());
    CHECK(rUnit.GetHomeBase() == &base);
    CHECK(rUnit.GetProducedAtBase() == &base);
    CHECK(&rUnit.GetDesign() == &rDesign);
}

TEST_CASE("CreateBaseFromSnapshot restores a queued unit design", "[production][unit][snapshot]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(3, 3);
    const UnitDesign& rDesign = game.AddDesign({"test_chassis", "test_weapon", "test_armor"});

    base.GetProduction().SetProduction(&rDesign);
    base.GetProduction().SetMineralStockpile(7);

    const BaseSnapshot_t snapshot = base.CaptureSnapshot();
    CHECK(snapshot.productionItemId == rDesign.GetId());
    CHECK(snapshot.mineralStockpile == 7);

    REQUIRE(game.pFaction->ExtractBase(base.GetBaseId()).has_value());

    BaseManager* pRestored = game.pFaction->CreateBaseFromSnapshot(
        snapshot, game.fixtures.dataContext, game.pState->GetTileEffects(),
        game.pState->GetSecretProjectAvailability());
    REQUIRE(pRestored != nullptr);
    REQUIRE(pRestored->GetProduction().GetCurrentProduction() != nullptr);
    CHECK(pRestored->GetProduction().GetCurrentProduction()->GetId() == rDesign.GetId());
    CHECK(pRestored->GetProduction().GetMineralStockpile() == 7);
}

TEST_CASE("Transfer clears queued building when the new owner lacks its required tech",
          "[production][transfer][tech-gate]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(2, 2);
    const BuildingConfig_t* pGated = game.fixtures.buildings().Find("test_gated_facility");
    REQUIRE(pGated != nullptr);
    REQUIRE_FALSE(pGated->IsAvailable(game.pOther->GetResearch().GetDiscoveredTechs()));

    game.pFaction->GetResearch().AddDiscoveredTech("advanced_build");
    base.GetProduction().SetProduction(pGated);
    base.GetProduction().SetMineralStockpile(12);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    CHECK_FALSE(base.GetProduction().HasProduction());
    CHECK(base.GetProduction().GetMineralStockpile() == 12);
}

TEST_CASE("Transfer keeps queued building when the new owner has its required tech",
          "[production][transfer][tech-gate]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(2, 2);
    const BuildingConfig_t* pGated = game.fixtures.buildings().Find("test_gated_facility");
    REQUIRE(pGated != nullptr);

    game.pFaction->GetResearch().AddDiscoveredTech("advanced_build");
    game.pOther->GetResearch().AddDiscoveredTech("advanced_build");
    base.GetProduction().SetProduction(pGated);
    base.GetProduction().SetMineralStockpile(12);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    REQUIRE(base.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction()->GetId() == pGated->id);
    CHECK(base.GetProduction().GetMineralStockpile() == 12);
}

TEST_CASE("Transfer clears queued unit design when the new owner lacks component techs",
          "[production][unit][transfer][tech-gate]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(5, 5);
    const std::vector<std::string> parts = {
        "test_chassis", "test_weapon", "test_armor", "test_gated_ability"};
    const UnitDesign& rGiverDesign = game.AddDesign(*game.pFaction, parts);
    const UnitDesign& rReceiverDesign = game.AddDesign(*game.pOther, parts);
    REQUIRE(rGiverDesign.GetId() == rReceiverDesign.GetId());

    game.pFaction->GetResearch().AddDiscoveredTech("advanced_build");
    REQUIRE(rGiverDesign.IsAvailable(game.pFaction->GetResearch().GetDiscoveredTechs()));
    REQUIRE_FALSE(rReceiverDesign.IsAvailable(game.pOther->GetResearch().GetDiscoveredTechs()));

    base.GetProduction().SetProduction(&rGiverDesign);
    base.GetProduction().SetMineralStockpile(9);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    CHECK_FALSE(base.GetProduction().HasProduction());
    CHECK(base.GetProduction().GetMineralStockpile() == 9);
}

TEST_CASE("Transfer rebinds queued unit design when the new owner has it and its techs",
          "[production][unit][transfer][tech-gate]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(5, 5);
    const std::vector<std::string> parts = {
        "test_chassis", "test_weapon", "test_armor", "test_gated_ability"};
    const UnitDesign& rGiverDesign = game.AddDesign(*game.pFaction, parts);
    const UnitDesign& rReceiverDesign = game.AddDesign(*game.pOther, parts);

    game.pFaction->GetResearch().AddDiscoveredTech("advanced_build");
    game.pOther->GetResearch().AddDiscoveredTech("advanced_build");

    base.GetProduction().SetProduction(&rGiverDesign);
    base.GetProduction().SetMineralStockpile(9);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    REQUIRE(base.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction() == &rReceiverDesign);
    CHECK(base.GetProduction().GetMineralStockpile() == 9);
}
