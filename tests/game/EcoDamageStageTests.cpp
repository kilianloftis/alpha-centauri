// Session-level ecology: the EcoDamage stage's roll and fungal pop, and the permanent
// clean-mineral grant on building completion.

#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/ecology/EcologyLedger.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/stages/EcoDamage.h"
#include "game/stages/WorldEvents.h"
#include "lib/EventBus.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_LifeformsPerBloom = 1;

// A live session on its own 9x9 land map: a player faction, an AI faction and the native-life
// faction a fungal bloom spawns lifeforms for.
struct EcoSession_
{
    FactionFixture fixtures;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;
    Faction* pPlanet = nullptr;

    EcoSession_()
    {
        InstallNativeUnits(fixtures.dataContext, k_LifeformsPerBloom, k_LifeformsPerBloom);
        pState = MakeLandSession(fixtures);
        pPlayer = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition,
                                     /*bIsPlayerControlled=*/true);
        pAi = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition,
                                 /*bIsPlayerControlled=*/false);
        pPlanet = &AddNativeLifeFaction(fixtures, *pState);
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        return MakeSessionBase(fixtures, *pState, rFaction, x, y);
    }

    Tile* At(int x, int y) { return pState->GetWorldMap().GetTile(x, y); }

    EcoDamageConfig_t& Eco() { return *fixtures.dataContext.ecoDamageConfig; }
    const EcologyLedger& Ecology() const { return pState->GetEcologyLedger(); }

    void RunEcoDamage(Faction& rFaction)
    {
        EcoDamage stage(HookContext("EcoDamage"));
        REQUIRE(stage.Execute(*pState, rFaction) == StageResult_t::Continue);
    }

    void RunWorldEvents(int yearsSinceFirstPlayable)
    {
        pState->SetMissionYear(GameState::k_FirstPlayableMissionYear + yearsSinceFirstPlayable);
        WorldEvents stage(HookContext("WorldEvents"));
        REQUIRE(stage.Execute(*pState) == StageResult_t::Continue);
    }

    std::vector<const Tile*> FungusTiles() const
    {
        std::vector<const Tile*> result;
        for (const auto& pTile : pState->GetWorldMap().GetTiles())
        {
            if (pTile->HasTerrainFeature(ImprovementIds::k_Fungus))
            {
                result.push_back(pTile.get());
            }
        }
        return result;
    }

    int UnitCount(const Faction& rFaction) const
    {
        int count = 0;
        for (const Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            (void)rUnit;
            ++count;
        }
        return count;
    }
};

bool InWorkableArea_(const BaseManager& rBase, const Tile& rTile)
{
    const auto& rTiles = rBase.GetWorkerAssignments().GetWorkableTiles();
    return std::find(rTiles.begin(), rTiles.end(), &rTile) != rTiles.end();
}

} // namespace

TEST_CASE("A base at 100% eco damage blooms inside its radius", "[ecology][stage]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "100";
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);
    std::vector<EvFungalBloom> events;
    session.pState->GetEventBus().Subscribe<EvFungalBloom>(
        std::function<void(const EvFungalBloom&)>(
            [&events](const EvFungalBloom& rEvent) { events.push_back(rEvent); }));
    const int lifeformsBefore = session.UnitCount(*session.pPlanet);

    session.RunEcoDamage(*session.pPlayer);

    CHECK(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 1);
    REQUIRE(events.size() == 1);
    CHECK(events[0].factionId == session.pPlayer->GetFactionId());
    CHECK(events[0].baseId == rBase.GetBaseId());

    const std::vector<const Tile*> fungus = session.FungusTiles();
    REQUIRE(fungus.size() == 1);
    CHECK(fungus[0] != &rBase.GetTile());
    CHECK(InWorkableArea_(rBase, *fungus[0]));
    CHECK(session.UnitCount(*session.pPlanet) >= lifeformsBefore + k_LifeformsPerBloom);

    PlayerInteractionQueue& rQueue = session.pState->GetPlayerInteractions();
    REQUIRE_FALSE(rQueue.Empty());
    const auto* pNotice = std::get_if<NoticeInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->event == PauseOnEventId_t::FungalBloom);
    CHECK(pNotice->cameraTile == TileCoord_t{fungus[0]->GetX(), fungus[0]->GetY()});
}

TEST_CASE("A base at 0% eco damage never blooms", "[ecology][stage]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "0";
    session.MakeBase(*session.pPlayer, 8, 8);

    session.RunEcoDamage(*session.pPlayer);

    CHECK(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 0);
    CHECK(session.FungusTiles().empty());
    CHECK(session.pState->GetPlayerInteractions().Empty());
}

TEST_CASE("max_chance_percent caps the pop roll", "[ecology][stage]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "100";
    session.Eco().maxChancePercent = 0;
    session.MakeBase(*session.pPlayer, 8, 8);

    session.RunEcoDamage(*session.pPlayer);

    CHECK(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 0);
}

TEST_CASE("An AI bloom is recorded but not announced to the player", "[ecology][stage]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "100";
    session.MakeBase(*session.pAi, 8, 8);

    session.RunEcoDamage(*session.pAi);

    CHECK(session.Ecology().FungalBlooms(session.pAi->GetFactionId()) == 1);
    CHECK(session.pState->GetPlayerInteractions().Empty());
}

TEST_CASE("The pop tile is never one that already has fungus", "[ecology][stage]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "100";
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);
    const Tile* pFree = session.At(10, 10);
    for (const Tile* pTile : rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (pTile != pFree)
        {
            session.pState->GetTileEffects().AddOccupantWithEffects(
                *session.At(pTile->GetX(), pTile->GetY()), std::string(ImprovementIds::k_Fungus));
        }
    }
    const std::size_t fungusBefore = session.FungusTiles().size();

    session.RunEcoDamage(*session.pPlayer);

    CHECK(pFree->HasTerrainFeature(ImprovementIds::k_Fungus));
    CHECK(session.FungusTiles().size() == fungusBefore + 1);
}

TEST_CASE("on_pop_effects applied without a pop tile plant nothing", "[ecology][stage]")
{
    EcoSession_ session;
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);

    TriggeredEffectContext_t context(*session.pState, rBase);
    context.pTile = nullptr;
    CHECK(ApplyTriggeredEffects(session.Eco().onPopEffects, context).empty());
    CHECK(session.FungusTiles().empty());
}

TEST_CASE("Each bloom raises the clean-minerals cap by one", "[ecology][stage][cap]")
{
    EcoSession_ session;
    session.Eco().damageFormula = "100";
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);

    session.RunEcoDamage(*session.pPlayer);
    session.RunEcoDamage(*session.pPlayer);
    REQUIRE(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 2);

    session.Eco().damageFormula = "clean_minerals + fungal_blooms + clean_mineral_grants";
    CHECK(rBase.GetEcologicalDamage() == 18);
}

TEST_CASE("A Perihelion starting this turn is in the stack when EcoDamage rolls",
          "[ecology][stage][world-events]")
{
    EcoSession_ session;
    session.pState->CreateWorldEvents();
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);
    const double baseline = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::EcologicalDamage,
                                            SeedFor(StatId_t::EcologicalDamage));
    session.Eco().damageFormula = "(eco_scale > " + std::to_string(baseline) + ") and 100 or 0";

    session.RunEcoDamage(*session.pPlayer);
    REQUIRE(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 0);

    session.RunWorldEvents(0);
    session.RunEcoDamage(*session.pPlayer);
    CHECK(session.Ecology().FungalBlooms(session.pPlayer->GetFactionId()) == 1);
}

TEST_CASE("Each eco facility completed after the first bloom grants one clean mineral",
          "[ecology][clean-minerals]")
{
    for (const char* pFacility :
         {"test_tree_farm", "test_hybrid_forest", "test_eco_preserve", "test_eco_temple"})
    {
        INFO(pFacility);
        EcoSession_ session;
        const FactionId_t factionId = session.pPlayer->GetFactionId();
        const BuildingConfig_t* pBuilding =
            session.fixtures.dataContext.buildingRegistry->Find(pFacility);
        REQUIRE(pBuilding);
        const auto complete = [&session, pBuilding](BaseManager& rBase) {
            rBase.GetProduction().SetProduction(pBuilding, rBase.GetBaseEffects());
            rBase.GetProduction().SetMineralStockpile(rBase.GetMineralCost());
            REQUIRE(rBase.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);
        };

        BaseManager& rBefore = session.MakeBase(*session.pPlayer, 8, 4);
        complete(rBefore);
        CHECK(session.Ecology().CleanMineralGrants(factionId) == 0);

        session.pState->GetEcologyLedger().RecordFungalBloom(factionId);
        BaseManager& rAfter = session.MakeBase(*session.pPlayer, 8, 12);
        complete(rAfter);
        CHECK(session.Ecology().CleanMineralGrants(factionId) == 1);

        // Permanent: scrapping the facility keeps the grant.
        rAfter.GetBuildingManager().DestroyBuilding(pFacility);
        CHECK(session.Ecology().CleanMineralGrants(factionId) == 1);
    }
}

TEST_CASE("A facility that arrives without being built grants nothing", "[ecology][clean-minerals]")
{
    EcoSession_ session;
    const FactionId_t factionId = session.pPlayer->GetFactionId();
    session.pState->GetEcologyLedger().RecordFungalBloom(factionId);
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);

    // The direct add behind base capture and granted buildings.
    rBase.GetBuildingManager().AddBuilding("test_tree_farm");
    CHECK(session.Ecology().CleanMineralGrants(factionId) == 0);

    TriggeredEffectConfig_t add;
    add.effect = AddBuildingEffect_t{"test_hybrid_forest"};
    TriggeredEffectContext_t context(*session.pState, rBase);
    ApplyTriggeredEffects(std::vector<TriggeredEffectConfig_t>{add}, context);
    REQUIRE(rBase.GetBuildingManager().HasBuilding("test_hybrid_forest"));
    CHECK(session.Ecology().CleanMineralGrants(factionId) == 0);
}

TEST_CASE("A Nanoreplicator raises EcoDamageReduction but never the cap",
          "[ecology][clean-minerals]")
{
    EcoSession_ session;
    const FactionId_t factionId = session.pPlayer->GetFactionId();
    session.pState->GetEcologyLedger().RecordFungalBloom(factionId);
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);
    session.Eco().damageFormula = "damage_reduction";
    const BuildingConfig_t* pNano =
        session.fixtures.dataContext.buildingRegistry->Find("test_nanoreplicator");
    REQUIRE(pNano);

    rBase.GetProduction().SetProduction(pNano, rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(rBase.GetMineralCost());
    REQUIRE(rBase.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);

    CHECK(session.Ecology().CleanMineralGrants(factionId) == 0);
    CHECK(rBase.GetEcologicalDamage() == 1);
}

TEST_CASE("A grant credited mid-turn moves the score", "[ecology][clean-minerals][memo]")
{
    EcoSession_ session;
    const FactionId_t factionId = session.pPlayer->GetFactionId();
    session.pState->GetEcologyLedger().RecordFungalBloom(factionId);
    BaseManager& rBase = session.MakeBase(*session.pPlayer, 8, 8);
    BaseManager& rBuilder = session.MakeBase(*session.pPlayer, 8, 14);
    session.Eco().damageFormula = "clean_mineral_grants";
    CHECK(rBase.GetEcologicalDamage() == 0);

    const BuildingConfig_t* pTemple =
        session.fixtures.dataContext.buildingRegistry->Find("test_eco_temple");
    REQUIRE(pTemple);
    rBuilder.GetProduction().SetProduction(pTemple, rBuilder.GetBaseEffects());
    rBuilder.GetProduction().SetMineralStockpile(rBuilder.GetMineralCost());
    REQUIRE(rBuilder.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);

    CHECK(rBase.GetEcologicalDamage() == 1);
}
