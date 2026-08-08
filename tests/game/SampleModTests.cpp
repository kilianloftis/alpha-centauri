// One worked mod, end to end, as the consumer the mod seams did not have. It uses both halves:
// a turn-stage hook (config-declared lifecycle) and an EventBus subscription (the stable
// mod-facing ABI). If either seam regresses — a hook that cannot see the game, an event that is
// declared but never published — these tests fail instead of the first modder finding out.

#include "GameFixtures.h"

#include "game/EventBridge.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/HookContext.h"
#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "lib/EventBus.h"
#include "lib/GameEvent.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

// Advance() requires the cycle to reach a yielding stage (the misconfiguration guard), so the
// harness ends the turn with one — the same shape as the real PlayerActions stage.
class AlwaysYieldStage_ : public GlobalTurnStage
{
public:
    AlwaysYieldStage_() : GlobalTurnStage(HookContext{}) {}

protected:
    StageResult_t ExecuteImpl(GameState&) override { return StageResult_t::Yield; }
};

// A stage that does nothing itself, so what the tests observe is the hooks.
class InertPerFactionStage : public PerFactionTurnStage
{
public:
    explicit InertPerFactionStage(HookContext hookContext)
        : PerFactionTurnStage(std::move(hookContext))
    {
    }

    int executeCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&, Faction&) override
    {
        ++executeCount;
        return StageResult_t::Continue;
    }
};

struct ModHarness_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pFaction = nullptr;

    ModHarness_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (const auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, k_TestRngSeed);

        pFaction = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed));
    }

    BaseManager& MakeBase(int x, int y)
    {
        BaseManager* pBase = pFaction->CreateBase(
            pState->AllocateBaseId(), "ModBase", pState->GetWorldMap().GetTile(x, y),
            fixtures.dataContext, pState->GetTileEffects(),
            pState->GetSecretProjectAvailability());
        REQUIRE(pBase != nullptr);
        return *pBase;
    }
};

} // namespace

TEST_CASE("A turn-stage hook is handed the game it is supposed to act on", "[mod][hooks]")
{
    // Hook_t::callback used to be std::function<void()>. A config-declared hook has captured
    // nothing, so with no arguments it could observe nothing and change nothing — the seam
    // existed but could not host a consumer.
    ModHarness_ harness;

    std::vector<std::string> seenStages;
    std::vector<FactionId_t> seenFactions;
    int seenBaseCount = -1;

    Hook_t preHook;
    preHook.modId = "sample_mod";
    preHook.callback = [&](const HookArgs_t& rArgs)
    {
        seenStages.push_back(rArgs.stageId);
        // A pre-hook fires on stage entry, which is not per faction.
        CHECK(rArgs.pFaction == nullptr);
        for (const Faction& rSeen : rArgs.rGameState.Factions())
        {
            seenBaseCount = static_cast<int>(rSeen.GetBaseCount());
        }
    };

    Hook_t replaceHook;
    replaceHook.modId = "sample_mod";
    replaceHook.callback = [&](const HookArgs_t& rArgs)
    {
        // A replace hook on a per-faction stage sees the faction being processed.
        REQUIRE(rArgs.pFaction != nullptr);
        seenFactions.push_back(rArgs.pFaction->GetFactionId());
    };

    harness.MakeBase(4, 4);

    HookContext hookContext("Population");
    hookContext.AddPreHook(preHook);
    hookContext.AddReplaceHook(replaceHook);

    auto pStage = std::make_unique<InertPerFactionStage>(std::move(hookContext));
    InertPerFactionStage* pRaw = pStage.get();

    PerFactionTurnStageRegistry_t perFaction;
    perFaction.emplace("Population", std::move(pStage));

    GlobalTurnStageRegistry_t global;
    global.emplace("PlayerActions", std::make_unique<AlwaysYieldStage_>());

    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"Population", "PlayerActions"});

    processor.Advance(*harness.pState);

    CHECK(seenStages == std::vector<std::string>{"Population"});
    CHECK(seenBaseCount == 1);
    CHECK(seenFactions == std::vector<FactionId_t>{harness.pFaction->GetFactionId()});
    // A callable replace hook suppresses the built-in stage body.
    CHECK(pRaw->executeCount == 0);
}

TEST_CASE("A mod observing the EventBus sees tech, base and riot events", "[mod][events]")
{
    // These three are declared in the mod-facing catalogue and were never published; the
    // architecture doc claimed they were bridged.
    ModHarness_ harness;
    EventBridge bridge(harness.pState->GetEventBus());
    bridge.WireFaction(*harness.pFaction);

    std::vector<TechId> discovered;
    std::vector<BaseId_t> built;
    int riots = 0;

    harness.pState->GetEventBus().Subscribe([&](const GameEvent& rEvent)
    {
        if (const auto* pTech = std::get_if<EvTechDiscovered>(&rEvent))
        {
            CHECK(pTech->factionId == harness.pFaction->GetFactionId());
            discovered.push_back(pTech->techId);
        }
        else if (const auto* pBase = std::get_if<EvBaseBuilt>(&rEvent))
        {
            CHECK(pBase->factionId == harness.pFaction->GetFactionId());
            built.push_back(pBase->baseId);
        }
        else if (std::get_if<EvDroneRiot>(&rEvent))
        {
            ++riots;
        }
    });

    harness.pFaction->GetResearch().AddDiscoveredTech("build_tech");
    CHECK(discovered == std::vector<TechId>{"build_tech"});

    BaseManager& rBase = harness.MakeBase(4, 4);
    CHECK(built == std::vector<BaseId_t>{rBase.GetBaseId()});

    // Riots reach the bus through the base wiring, which OnBaseAdded drives in the engine; the
    // harness wires it explicitly because it has no Engine.
    bridge.WireBase(rBase);
    rBase.GetPopulation().OnIsRioting.Emit();
    CHECK(riots == 1);
}

TEST_CASE("Wiring the same faction twice does not double-publish", "[mod][events]")
{
    // WireBase is idempotent by object; WireFaction must match, or a re-wired faction delivers
    // every event twice to every mod.
    ModHarness_ harness;
    EventBridge bridge(harness.pState->GetEventBus());
    bridge.WireFaction(*harness.pFaction);
    bridge.WireFaction(*harness.pFaction);

    int techEvents = 0;
    harness.pState->GetEventBus().Subscribe([&](const GameEvent& rEvent)
    {
        if (std::get_if<EvTechDiscovered>(&rEvent))
        {
            ++techEvents;
        }
    });

    harness.pFaction->GetResearch().AddDiscoveredTech("build_tech");
    CHECK(techEvents == 1);
}

TEST_CASE("A mod can replace the tile-scoring policy", "[mod][hooks]")
{
    // SetTileScorer is a declared customization seam with no consumer, which is how the other
    // seams in this package came to be half-built. Exercising it here means a change that
    // breaks it fails the suite.
    ModHarness_ harness;
    BaseManager& rBase = harness.MakeBase(4, 4);

    int scored = 0;
    const Tile* pPreferred = rBase.GetWorkerAssignments().GetWorkableTiles().front();

    rBase.GetWorkerAssignments().SetTileScorer(
        [&](const Tile& rTile)
        {
            ++scored;
            // Rank the mod's chosen tile above everything else.
            return &rTile == pPreferred ? 1000 : 0;
        });
    rBase.GetWorkerAssignments().ResetAllAssignments();

    CHECK(scored > 0);
    CHECK(rBase.GetWorkerAssignments().IsTileWorkedByThisBase(pPreferred));
}
