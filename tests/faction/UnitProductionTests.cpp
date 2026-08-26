// Unit designs are IConstructable and appear in a base's production list; completing one
// spawns the unit on the base tile with home / produced-at set to that base.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/HookContext.h"
#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/stages/BaseProduction.h"
#include "game/stockpiles/StockpileConfig.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
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

class AlwaysYieldStage_ : public GlobalTurnStage
{
public:
    using GlobalTurnStage::GlobalTurnStage;
protected:
    StageResult_t ExecuteImpl(GameState&) override { return StageResult_t::Yield; }
};

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
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules, k_TestRngSeed);

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

const StockpileConfig_t* StockpileEnergyOf_(UnitProductionGame_& rGame)
{
    const StockpileConfig_t* pStockpile = rGame.fixtures.stockpiles().Find("Stockpile_Energy");
    REQUIRE(pStockpile != nullptr);
    return pStockpile;
}

void CheckQueuedStockpileEnergy_(const BaseManager& rBase, UnitProductionGame_& rGame)
{
    CHECK(rBase.GetProduction().GetCurrentProduction() == StockpileEnergyOf_(rGame));
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

TEST_CASE("Completing colony pod production decreases base population by 1",
          "[production][unit][population]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    REQUIRE(base.GetPopulation().GetSize() == 3);

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    const ProductionApplyResult_t applied = base.ApplyProduction();
    CHECK(applied.kind == ProductionApplyKind_t::Completed);
    CHECK(applied.completedId == rPod.GetId());
    CHECK(base.GetPopulation().GetSize() == 2);

    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile());
    REQUIRE(onTile.size() == 1);
    CHECK(onTile.front()->GetFlag(RuleFlagId_t::FoundBase));
}

TEST_CASE("Colony pod that would empty the base opens abandon confirmation",
          "[production][unit][population][abandon]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    while (base.GetPopulation().GetSize() > 1)
    {
        base.GetPopulation().RemovePop();
    }
    REQUIRE(base.GetPopulation().GetSize() == 1);

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    const ProductionApplyResult_t applied = base.ApplyProduction();
    CHECK(applied.kind == ProductionApplyKind_t::AwaitingAbandonConfirm);
    CHECK(base.HasPendingProductionAbandonConfirm());
    CHECK(base.GetPopulation().GetSize() == 1);
    CHECK(base.GetProduction().HasProduction());
    CHECK(base.GetProduction().GetMineralStockpile() >= base.GetMineralCost());
    CHECK(game.pFaction->GetUnitManager().Units().empty());
}

TEST_CASE("ConfirmProductionAbandon completes the unit and empties the base",
          "[production][unit][population][abandon]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    while (base.GetPopulation().GetSize() > 1)
    {
        base.GetPopulation().RemovePop();
    }

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::AwaitingAbandonConfirm);
    REQUIRE(base.HasPendingProductionAbandonConfirm());

    CHECK(base.ConfirmProductionAbandon() == rPod.GetId());
    CHECK_FALSE(base.HasPendingProductionAbandonConfirm());
    CHECK(base.GetPopulation().GetSize() == 0);
    CheckQueuedStockpileEnergy_(base, game);
    CHECK(std::ranges::distance(game.pFaction->GetUnitManager().Units()) == 1);
}

TEST_CASE("DeferProductionAbandon keeps the base and loses minerals",
          "[production][unit][population][abandon]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    while (base.GetPopulation().GetSize() > 1)
    {
        base.GetPopulation().RemovePop();
    }

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost() + 5);
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::AwaitingAbandonConfirm);
    REQUIRE(base.HasPendingProductionAbandonConfirm());

    base.DeferProductionAbandon();
    CHECK_FALSE(base.HasPendingProductionAbandonConfirm());
    CHECK(base.GetPopulation().GetSize() == 1);
    CHECK(base.GetProduction().HasProduction());
    CHECK(base.GetProduction().GetCurrentProduction() == &rPod);
    CHECK(base.GetProduction().GetMineralStockpile() == 0);
    CHECK(game.pFaction->GetUnitManager().Units().empty());
}

TEST_CASE("CreateUnit without production does not apply Instantaneous component effects",
          "[production][unit][population]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    REQUIRE(base.GetPopulation().GetSize() == 3);

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    game.pFaction->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), rPod, game.pState->GetWorldMap().GetUnitPositions(),
        base.GetTile(), &base, &base);

    CHECK(base.GetPopulation().GetSize() == 3);
}

TEST_CASE("BaseProduction yields for player abandon confirm and resumes after defer",
          "[production][BaseProduction][abandon]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    while (base.GetPopulation().GetSize() > 1)
    {
        base.GetPopulation().RemovePop();
    }

    const UnitDesign& rPod =
        game.AddDesign({"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["BaseProduction"] = std::make_unique<BaseProduction>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>(HookContext{});
    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"BaseProduction", "Stop"});

    processor.Advance(*game.pState);
    CHECK(base.HasPendingProductionAbandonConfirm());
    CHECK(base.GetPopulation().GetSize() == 1);
    REQUIRE(game.pState->GetPlayerInteractions().Front());
    CHECK(std::holds_alternative<ProductionAbandonInteraction_t>(
        game.pState->GetPlayerInteractions().Front()->payload));

    base.DeferProductionAbandon();
    game.pState->GetPlayerInteractions().CompleteFront();
    processor.Advance(*game.pState);

    CHECK_FALSE(base.HasPendingProductionAbandonConfirm());
    CHECK(game.pState->GetPlayerInteractions().Empty());
    CHECK(base.GetPopulation().GetSize() == 1);
    CHECK(base.GetProduction().GetMineralStockpile() == 0);
    CHECK(base.GetProduction().HasProduction());
}

TEST_CASE("BaseProduction AI auto-defers abandon without yielding",
          "[production][BaseProduction][abandon]")
{
    UnitProductionGame_ game;
    // pOther is not player-controlled.
    BaseManager& base = game.MakeBase(*game.pOther, 4, 4);
    while (base.GetPopulation().GetSize() > 1)
    {
        base.GetPopulation().RemovePop();
    }

    const UnitDesign& rPod =
        game.AddDesign(*game.pOther, {"test_chassis", "test_colony_pod", "test_armor"});
    base.GetProduction().SetProduction(&rPod, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    BaseProduction stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pOther) == StageResult_t::Continue);
    CHECK_FALSE(base.HasPendingProductionAbandonConfirm());
    CHECK(base.GetPopulation().GetSize() == 1);
    CHECK(base.GetProduction().GetMineralStockpile() == 0);
    CHECK(game.pOther->GetUnitManager().Units().empty());
}

TEST_CASE("BaseProduction enqueues an idle prompt after completion and queues Stockpile Energy",
          "[production][BaseProduction][PlayerInteraction]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_weapon", "test_armor"});
    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["BaseProduction"] = std::make_unique<BaseProduction>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>(HookContext{});
    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"BaseProduction", "Stop"});

    processor.Advance(*game.pState);

    REQUIRE(game.pState->GetPlayerInteractions().Size() == 1);
    {
        const auto& rIdle = std::get<ProductionIdleInteraction_t>(
            game.pState->GetPlayerInteractions().Front()->payload);
        CHECK(rIdle.afterCompletion);
        CHECK(rIdle.completedEvent == PauseOnEventId_t::PrototypeBuilt);
        CHECK(rIdle.baseId == base.GetBaseId());
        CHECK(rIdle.completedItemId == rDesign.GetId());
        CHECK(rIdle.completedItemName == rDesign.GetId());
    }

    game.pState->GetPlayerInteractions().CompleteFront();
    processor.Advance(*game.pState);
    CHECK(game.pState->GetPlayerInteractions().Empty());
    CheckQueuedStockpileEnergy_(base, game);
}

TEST_CASE("Completing unit production places the unit on the base tile", "[production][unit]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign = game.AddDesign({"test_chassis", "test_weapon", "test_armor"});

    REQUIRE(game.pFaction->GetUnitManager().Units().empty());
    REQUIRE(game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile()).empty());

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    REQUIRE(base.GetMineralCost() >= 1);
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());

    const ProductionApplyResult_t applied = base.ApplyProduction();
    CHECK(applied.kind == ProductionApplyKind_t::Completed);
    CHECK(applied.completedId == rDesign.GetId());
    CheckQueuedStockpileEnergy_(base, game);

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

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
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
    base.GetProduction().SetProduction(pGated, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(12);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    CheckQueuedStockpileEnergy_(base, game);
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
    base.GetProduction().SetProduction(pGated, base.GetBaseEffects());
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

    base.GetProduction().SetProduction(&rGiverDesign, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(9);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    CheckQueuedStockpileEnergy_(base, game);
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

    base.GetProduction().SetProduction(&rGiverDesign, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(9);

    game.pFaction->TransferBaseTo(base.GetBaseId(), *game.pOther);

    REQUIRE(base.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(base.GetProduction().GetCurrentProduction() == &rReceiverDesign);
    CHECK(base.GetProduction().GetMineralStockpile() == 9);
}

TEST_CASE("A unit is a prototype when any component is new to the faction",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    REQUIRE(game.pFaction->GetMilitary().IsPrototype(rDesign));

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    const int prototypeCost = base.GetMineralCost();
    const int expected =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{base}, 50);
    CHECK(rDesign.GetBaseCost() == 40);
    CHECK(prototypeCost == expected);
    CHECK(prototypeCost > rDesign.GetBaseCost());

    base.GetProduction().SetMineralStockpile(prototypeCost);
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    CHECK_FALSE(game.pFaction->GetMilitary().IsPrototype(rDesign));
    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile());
    REQUIRE(onTile.size() == 1);
    CHECK(onTile.front()->GetXp() == 2);
}

TEST_CASE("Skunkworks cancels prototype mineral surcharge but not prototype XP",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& withSkunk = game.MakeBase(2, 2);
    BaseManager& without = game.MakeBase(6, 6);
    withSkunk.GetBuildingManager().AddBuilding("Skunkworks");
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    withSkunk.GetProduction().SetProduction(&rDesign, withSkunk.GetBaseEffects());
    without.GetProduction().SetProduction(&rDesign, without.GetBaseEffects());

    const int standardCost =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{withSkunk}, 0);
    const int prototypeCost =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{without}, 50);
    REQUIRE(prototypeCost > standardCost);

    CHECK(withSkunk.GetMineralCost() == standardCost);
    CHECK(without.GetMineralCost() == prototypeCost);

    withSkunk.GetProduction().SetMineralStockpile(standardCost);
    REQUIRE(withSkunk.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(withSkunk.GetTile());
    REQUIRE(onTile.size() == 1);
    CHECK(onTile.front()->IsPrototype());
    CHECK(onTile.front()->GetXp() == 2);
    CHECK_FALSE(game.pFaction->GetMilitary().IsPrototype(rDesign));
}

TEST_CASE("Skunkworks cancels retool penalty", "[production][retool][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    base.GetBuildingManager().AddBuilding("Skunkworks");
    const UnitDesign& rFirst =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});
    const UnitDesign& rSecond =
        game.AddDesign({"test_chassis", "test_costly_weapon_alt", "test_costly_armor"});

    base.GetProduction().SetProduction(&rFirst, base.GetBaseEffects());
    base.GetProduction().BankProduction(0);
    base.GetProduction().SetMineralStockpile(40);

    base.GetProduction().SetProduction(&rSecond, base.GetBaseEffects());
    CHECK(base.GetProduction().GetMineralStockpile() == 40);
}

TEST_CASE("Prototype StartingExperience stacks with ProducedAtThisBase train bonuses",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    base.GetBuildingManager().AddBuilding("Aerospace_Complex");
    const UnitDesign& rDesign =
        game.AddDesign({"test_flight_chassis", "test_costly_weapon", "test_costly_armor"});

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile());
    REQUIRE(onTile.size() == 1);
    CHECK(onTile.front()->GetXp() == 4);
}

TEST_CASE("Several new components still apply a single prototype surcharge",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});
    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());

    CHECK(base.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{base}, 50));
    CHECK(base.GetMineralCost()
          != ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{base}, 150));
}

TEST_CASE("Fielding a unit removes the prototype penalty from other queues of that faction",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& first = game.MakeBase(2, 2);
    BaseManager& second = game.MakeBase(6, 6);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    first.GetProduction().SetProduction(&rDesign, first.GetBaseEffects());
    second.GetProduction().SetProduction(&rDesign, second.GetBaseEffects());
    const int prototypeCost = first.GetMineralCost();
    const int standardCost =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{first}, 0);
    REQUIRE(prototypeCost > standardCost);

    first.GetProduction().SetMineralStockpile(standardCost);
    CHECK(first.ApplyProduction().kind == ProductionApplyKind_t::InProgress);

    second.GetProduction().SetMineralStockpile(prototypeCost);
    const ProductionApplyResult_t finished = second.ApplyProduction();
    REQUIRE(finished.kind == ProductionApplyKind_t::Completed);
    CHECK(finished.completedEvent == PauseOnEventId_t::PrototypeBuilt);

    CHECK(first.GetMineralCost() == standardCost);
    CHECK(first.GetProduction().HasProduction());
    CHECK(first.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);
    CheckQueuedStockpileEnergy_(first, game);
}

TEST_CASE("A remaining unbuilt component keeps other designs as prototypes",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rFirst =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});
    const UnitDesign& rSecond =
        game.AddDesign({"test_chassis", "test_costly_weapon_alt", "test_costly_armor"});

    base.GetProduction().SetProduction(&rFirst, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    CHECK_FALSE(game.pFaction->GetMilitary().IsPrototype(rFirst));
    REQUIRE(game.pFaction->GetMilitary().IsPrototype(rSecond));

    BaseManager& other = game.MakeBase(6, 6);
    other.GetProduction().SetProduction(&rSecond, other.GetBaseEffects());
    CHECK(other.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(rSecond.GetBaseCost(), BaseEffects_t{other}, 50));
}

TEST_CASE("Prototype knowledge is per faction", "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& playerBase = game.MakeBase(2, 2);
    BaseManager& aiBase = game.MakeBase(*game.pOther, 6, 6);
    const UnitDesign& rPlayer =
        game.AddDesign(*game.pFaction, {"test_chassis", "test_costly_weapon", "test_costly_armor"});
    const UnitDesign& rAi =
        game.AddDesign(*game.pOther, {"test_chassis", "test_costly_weapon", "test_costly_armor"});

    playerBase.GetProduction().SetProduction(&rPlayer, playerBase.GetBaseEffects());
    playerBase.GetProduction().SetMineralStockpile(playerBase.GetMineralCost());
    REQUIRE(playerBase.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    aiBase.GetProduction().SetProduction(&rAi, aiBase.GetBaseEffects());
    CHECK(game.pOther->GetMilitary().IsPrototype(rAi));
    CHECK(aiBase.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(rAi.GetBaseCost(), BaseEffects_t{aiBase}, 50));
}

TEST_CASE("CreateUnit applies prototype StartingExperience then unlocks the components",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    Unit& spawned = game.pFaction->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), rDesign, game.pState->GetWorldMap().GetUnitPositions(),
        *game.pState->GetWorldMap().GetTile(0, 0), &base, &base);
    CHECK(spawned.IsPrototype());
    CHECK(spawned.GetXp() == 2);
    CHECK(spawned.GetStat(StatId_t::StartingExperience) == 1);
    CHECK_FALSE(game.pFaction->GetMilitary().IsPrototype(rDesign));

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    CHECK(base.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{base}, 0));
}

TEST_CASE("Free CreateUnit does not latch prototype but still unlocks the ledger",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});
    REQUIRE(game.pFaction->GetMilitary().IsPrototype(rDesign));

    // Home only — Engine starting units / gift path. Not "built".
    Unit& gifted = game.pFaction->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), rDesign, game.pState->GetWorldMap().GetUnitPositions(),
        *game.pState->GetWorldMap().GetTile(0, 0), &base);
    CHECK_FALSE(gifted.IsPrototype());
    CHECK(gifted.GetXp() == 1);
    CHECK(gifted.GetStat(StatId_t::StartingExperience) == 0);
    CHECK_FALSE(game.pFaction->GetMilitary().IsPrototype(rDesign));

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    CHECK(base.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{base}, 0));
}

TEST_CASE("BaseProduction completes a sibling queue when a prototype finishes",
          "[production][unit][prototype][BaseProduction]")
{
    UnitProductionGame_ game;
    BaseManager& first = game.MakeBase(2, 2);
    BaseManager& second = game.MakeBase(6, 6);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    first.GetProduction().SetProduction(&rDesign, first.GetBaseEffects());
    second.GetProduction().SetProduction(&rDesign, second.GetBaseEffects());
    const int standardCost =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{first}, 0);
    const int prototypeCost = first.GetMineralCost();
    REQUIRE(prototypeCost > standardCost);

    first.GetProduction().SetMineralStockpile(standardCost);
    second.GetProduction().SetMineralStockpile(prototypeCost);

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["BaseProduction"] = std::make_unique<BaseProduction>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>(HookContext{});
    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"BaseProduction", "Stop"});

    processor.Advance(*game.pState);

    CheckQueuedStockpileEnergy_(first, game);
    CheckQueuedStockpileEnergy_(second, game);
    CHECK(std::ranges::distance(game.pFaction->GetUnitManager().Units()) == 2);
    CHECK(game.pState->GetPlayerInteractions().Size() >= 1);
}

TEST_CASE("A base that already ticked is not revisited when a sibling completion yields",
          "[production][unit][prototype][BaseProduction]")
{
    UnitProductionGame_ game;
    // Creation order is iteration order. The laggard ticks first and stalls on the surcharge;
    // the finisher then completes the prototype, which drops the laggard's cost and makes
    // ReevaluateProcessedBases_ complete it — yielding from inside the finisher's own tick.
    BaseManager& laggard = game.MakeBase(2, 2);
    BaseManager& finisher = game.MakeBase(6, 6);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    laggard.GetProduction().SetProduction(&rDesign, laggard.GetBaseEffects());
    finisher.GetProduction().SetProduction(&rDesign, finisher.GetBaseEffects());
    const int standardCost =
        ProductionCostCalculator::ComputeCost(rDesign.GetBaseCost(), BaseEffects_t{laggard}, 0);
    const int prototypeCost = finisher.GetMineralCost();
    REQUIRE(prototypeCost > standardCost);

    laggard.GetProduction().SetMineralStockpile(standardCost);
    finisher.GetProduction().SetMineralStockpile(prototypeCost);

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["BaseProduction"] = std::make_unique<BaseProduction>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>(HookContext{});
    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"BaseProduction", "Stop"});

    processor.Advance(*game.pState);
    CheckQueuedStockpileEnergy_(finisher, game);
    CheckQueuedStockpileEnergy_(laggard, game);

    while (!game.pState->GetPlayerInteractions().Empty())
    {
        game.pState->GetPlayerInteractions().CompleteFront();
    }

    REQUIRE(std::ranges::distance(game.pFaction->GetUnitManager().Units()) == 2);

    // The player answers the prompt by queueing something new at the base that already
    // completed this turn, and that base now has minerals in its resource bank. Resuming
    // the pass must not tick it a second time: doing so used to bank income twice in one
    // turn and complete a second unit off the back of it. ConvertMinerals already ran
    // earlier; ApplyProduction must still not consume the leftover bank.
    finisher.GetBuildingManager().AddBuilding("mineral_cache");
    finisher.GetProduction().SetProduction(&rDesign, finisher.GetBaseEffects());
    finisher.GetProduction().SetMineralStockpile(0);
    finisher.ProduceResources();
    const int bankedBefore = finisher.GetResources().GetMineralBank();
    REQUIRE(bankedBefore >= standardCost);

    processor.Advance(*game.pState);

    CHECK(finisher.GetResources().GetMineralBank() == bankedBefore);
    CHECK(finisher.GetProduction().GetMineralStockpile() == 0);
    CHECK(finisher.GetProduction().HasProduction());
    CHECK(std::ranges::distance(game.pFaction->GetUnitManager().Units()) == 2);
}

TEST_CASE("A unit keeps the prototype status it was built with after the ledger moves on",
          "[production][unit][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const UnitDesign& rDesign =
        game.AddDesign({"test_chassis", "test_costly_weapon", "test_costly_armor"});

    base.GetProduction().SetProduction(&rDesign, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    const std::vector<Unit*>& onTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(base.GetTile());
    REQUIRE(onTile.size() == 1);
    const Unit& rPrototype = *onTile.front();

    // The faction has fielded the design, so the ledger no longer calls it a prototype...
    REQUIRE_FALSE(game.pFaction->GetMilitary().IsPrototype(rDesign));
    // ...but the unit built as one still is, and a live re-resolve of the effect agrees with
    // the XP that was baked at construction rather than silently dropping to 0.
    CHECK(rPrototype.IsPrototype());
    CHECK(rPrototype.GetXp() == 2);
    CHECK(rPrototype.GetStat(StatId_t::StartingExperience) == 1);

    BaseManager& second = game.MakeBase(6, 6);
    second.GetProduction().SetProduction(&rDesign, second.GetBaseEffects());
    second.GetProduction().SetMineralStockpile(second.GetMineralCost());
    REQUIRE(second.ApplyProduction().kind == ProductionApplyKind_t::Completed);

    const std::vector<Unit*>& onSecondTile =
        game.pState->GetWorldMap().GetUnitPositions().GetUnitsOnTile(second.GetTile());
    REQUIRE(onSecondTile.size() == 1);
    CHECK_FALSE(onSecondTile.front()->IsPrototype());
    CHECK(onSecondTile.front()->GetXp() == 1);
    CHECK(onSecondTile.front()->GetStat(StatId_t::StartingExperience) == 0);
}

TEST_CASE("A facility never takes the prototype surcharge", "[production][prototype]")
{
    UnitProductionGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const BuildingConfig_t* pFacility = game.fixtures.buildings().Find("test_facility_a");
    REQUIRE(pFacility != nullptr);

    base.GetProduction().SetProduction(pFacility, base.GetBaseEffects());
    CHECK(base.GetMineralCost()
          == ProductionCostCalculator::ComputeCost(pFacility->GetBaseCost(), BaseEffects_t{base}, 0));
}
