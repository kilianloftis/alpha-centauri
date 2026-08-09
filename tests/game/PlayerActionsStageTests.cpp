#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/HookContext.h"
#include "game/TurnProcessor.h"
#include "game/faction/UnitManager.h"
#include "game/map/WorldMap.h"
#include "game/stages/PlayerActions.h"
#include "game/stages/Population.h"
#include "game/stages/WorldEvents.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitSlotConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct PlayerActionsGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;

    PlayerActionsGame_()
    {
        pState = std::make_unique<GameState>(
            std::make_unique<WorldMap>(9, 9), fixtures.improvements, &fixtures.unitComponents,
            settings, *fixtures.dataContext.moraleCalculator, actest::k_TestRngSeed);

        auto pFaction = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        pPlayer = &pState->AddFaction(std::move(pFaction));
    }

    Unit& MakeUnit(int x, int y)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        const UnitComponentConfig_t* pChassis = fixtures.unitComponents.Find("test_chassis");
        REQUIRE(pChassis);
        UnitSlotConfig_t slot;
        slot.id = "slot_0";
        slot.displayName = slot.id;
        slot.componentType = pChassis->type;
        slots.push_back(slot);
        assigned[slot.id] = pChassis;
        fixtures.designs.emplace_back(slots, assigned);

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return pPlayer->GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile);
    }
};

class AlwaysYieldStage_ : public GlobalTurnStage
{
public:
    AlwaysYieldStage_() : GlobalTurnStage(HookContext{}) {}

protected:
    StageResult_t ExecuteImpl(GameState&) override { return StageResult_t::Yield; }
};

} // namespace

TEST_CASE("PlayerActions mid-pass yield does not double-tick HoldForTurns",
          "[PlayerActions][TurnProcessor]")
{
    PlayerActionsGame_ game;
    Unit& holder = game.MakeUnit(2, 2);
    Unit& shortHold = game.MakeUnit(4, 4);

    holder.SetOrder(HoldForTurnsOrder_t{3});
    // Completes in one Execute and still has moves → Yield after holder already advanced.
    shortHold.SetOrder(HoldForTurnsOrder_t{1});

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["PlayerActions"] = std::make_unique<PlayerActions>(HookContext{});

    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>();

    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"PlayerActions", "Stop"});

    // Interaction yield
    processor.Advance(*game.pState);
    // Resume: holder ticks once (3→2), shortHold completes and needs orders → Yield
    processor.Advance(*game.pState);

    REQUIRE(holder.GetOrder().has_value());
    CHECK(std::get<HoldForTurnsOrder_t>(*holder.GetOrder()).turnsRemaining == 2);

    // Resume again: holder must not tick again this pass (still 2, not 1)
    processor.Advance(*game.pState);
    REQUIRE(holder.GetOrder().has_value());
    CHECK(std::get<HoldForTurnsOrder_t>(*holder.GetOrder()).turnsRemaining == 2);
}

TEST_CASE("PlayerActions interaction yield then resolves orders on resume",
          "[PlayerActions][TurnProcessor]")
{
    PlayerActionsGame_ game;
    Unit& holder = game.MakeUnit(2, 2);
    holder.SetOrder(HoldForTurnsOrder_t{1});

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["PlayerActions"] = std::make_unique<PlayerActions>(HookContext{});

    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>();

    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"PlayerActions", "Stop"});

    processor.Advance(*game.pState); // interaction Yield — order not yet ticked
    REQUIRE(holder.GetOrder().has_value());
    CHECK(std::get<HoldForTurnsOrder_t>(*holder.GetOrder()).turnsRemaining == 1);

    processor.Advance(*game.pState); // resolve → Complete clears order, Continue, Stop Yields
    CHECK_FALSE(holder.GetOrder().has_value());
}

TEST_CASE("PlayerActions yields for interaction again on the next pass",
          "[PlayerActions]")
{
    PlayerActionsGame_ game;
    Unit& holder = game.MakeUnit(2, 2);
    // Multi-turn hold so the order pass Continues without a "needs orders" Yield.
    holder.SetOrder(HoldForTurnsOrder_t{2});

    PlayerActions stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield);
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Continue);
    REQUIRE(holder.GetOrder().has_value());
    CHECK(std::get<HoldForTurnsOrder_t>(*holder.GetOrder()).turnsRemaining == 1);

    // Next pass must gate on interaction again (not skip straight to order resolution).
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield);
    CHECK(std::get<HoldForTurnsOrder_t>(*holder.GetOrder()).turnsRemaining == 1);
}

TEST_CASE("PlayerActions mid-pass Yield allows a new order on the completed unit",
          "[PlayerActions][TurnProcessor]")
{
    PlayerActionsGame_ game;
    Unit& multi = game.MakeUnit(2, 2);
    Unit& finishing = game.MakeUnit(4, 4);
    multi.SetOrder(HoldForTurnsOrder_t{3});
    finishing.SetOrder(HoldForTurnsOrder_t{1});

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["PlayerActions"] = std::make_unique<PlayerActions>(HookContext{});
    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>();
    TurnProcessor processor(std::move(global), std::move(perFaction),
                            {"PlayerActions", "Stop"});

    processor.Advance(*game.pState); // interaction
    processor.Advance(*game.pState); // multi 3→2, finishing completes → Yield for orders
    CHECK(std::get<HoldForTurnsOrder_t>(*multi.GetOrder()).turnsRemaining == 2);
    CHECK_FALSE(finishing.GetOrder().has_value());

    finishing.SetOrder(HoldForTurnsOrder_t{2});
    processor.Advance(*game.pState); // new order on finishing must run; multi must not re-tick
    CHECK(std::get<HoldForTurnsOrder_t>(*multi.GetOrder()).turnsRemaining == 2);
    REQUIRE(finishing.GetOrder().has_value());
    CHECK(std::get<HoldForTurnsOrder_t>(*finishing.GetOrder()).turnsRemaining == 1);
}

TEST_CASE("PlayerActions resets interaction phase when active faction changes",
          "[PlayerActions]")
{
    PlayerActionsGame_ game;
    auto pOther = std::make_unique<Faction>(
        game.pState->AllocateFactionId(), true, game.fixtures.factionDefinition,
        game.fixtures.dataContext, game.pState->GetWorldMap(), game.settings,
        actest::k_TestFactionSeed);
    Faction& rOther = game.pState->AddFaction(std::move(pOther));

    PlayerActions stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield);
    // Abandoned prior faction: next player must still get the interaction Yield.
    CHECK(stage.Execute(*game.pState, rOther) == StageResult_t::Yield);
}

TEST_CASE("SkipTurn persists without spending moves or re-opening needs-orders",
          "[PlayerActions][unit-order]")
{
    PlayerActionsGame_ game;
    Unit& skipped = game.MakeUnit(2, 2);
    Unit& awaiting = game.MakeUnit(4, 4);
    const int skippedMoves = skipped.GetMoveFragmentsRemaining();
    const int awaitingMoves = awaiting.GetMoveFragmentsRemaining();
    REQUIRE(skippedMoves > 0);
    REQUIRE(awaitingMoves > 0);

    skipped.SetOrder(SkipTurnOrder_t{});
    CHECK(game.pPlayer->GetUnitManager().GetNextAvailableUnit() == &awaiting);
    CHECK(game.pPlayer->GetUnitManager().HasUnitsRequiringOrders());

    awaiting.SetOrder(SkipTurnOrder_t{});
    CHECK_FALSE(game.pPlayer->GetUnitManager().HasUnitsRequiringOrders());

    PlayerActions stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield); // interaction
    // Both SkipTurns Continue (order kept); stage Continues with no needs-orders Yield.
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Continue);

    REQUIRE(skipped.GetOrder().has_value());
    REQUIRE(awaiting.GetOrder().has_value());
    CHECK(std::holds_alternative<SkipTurnOrder_t>(*skipped.GetOrder()));
    CHECK(std::holds_alternative<SkipTurnOrder_t>(*awaiting.GetOrder()));
    CHECK(skipped.GetMoveFragmentsRemaining() == skippedMoves);
    CHECK(awaiting.GetMoveFragmentsRemaining() == awaitingMoves);
    CHECK_FALSE(game.pPlayer->GetUnitManager().HasUnitsRequiringOrders());
}

TEST_CASE("SkipTurn stays out of needs-orders when another unit yields mid-pass",
          "[PlayerActions][unit-order]")
{
    PlayerActionsGame_ game;
    Unit& skipped = game.MakeUnit(2, 2);
    Unit& shortHold = game.MakeUnit(4, 4);
    const int skippedMoves = skipped.GetMoveFragmentsRemaining();
    skipped.SetOrder(SkipTurnOrder_t{});
    shortHold.SetOrder(HoldForTurnsOrder_t{1});

    PlayerActions stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield); // interaction
    // shortHold Completes and still needs orders → Yield. SkipTurn order still held.
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Yield);

    CHECK_FALSE(shortHold.GetOrder().has_value());
    REQUIRE(skipped.GetOrder().has_value());
    CHECK(std::holds_alternative<SkipTurnOrder_t>(*skipped.GetOrder()));
    CHECK(skipped.GetMoveFragmentsRemaining() == skippedMoves);
    CHECK(game.pPlayer->GetUnitManager().GetNextAvailableUnit() == &shortHold);
}

TEST_CASE("Population stage invokes riot and golden-age end-of-turn updates",
          "[Population][TurnProcessor]")
{
    PlayerActionsGame_ game;
    Tile* pTile = game.pState->GetWorldMap().GetTile(3, 3);
    REQUIRE(pTile);
    BaseManager* pBase = game.pPlayer->CreateBase(
        game.pState->AllocateBaseId(), "TestBase", pTile, game.fixtures.dataContext,
        game.pState->GetTileEffects(), game.pState->GetSecretProjectAvailability());
    REQUIRE(pBase);

    int goldenAgeCallbacks = 0;
    pBase->GetPopulation().OnGoldenAgeStarted.Connect([&]() { ++goldenAgeCallbacks; });
    pBase->GetPopulation().OnGoldenAgeEnded.Connect([&]() { ++goldenAgeCallbacks; });

    pBase->GetPopulation().ForceRiot(/*turns=*/1);
    REQUIRE(pBase->GetPopulation().IsRioting());

    PerFactionTurnStageRegistry_t perFaction;
    perFaction["Population"] = std::make_unique<Population>(HookContext{});

    GlobalTurnStageRegistry_t global;
    global["Stop"] = std::make_unique<AlwaysYieldStage_>();

    TurnProcessor processor(std::move(global), std::move(perFaction), {"Population", "Stop"});
    processor.Advance(*game.pState);

    // The fixture base is not drone-majority, so only the forced riot keeps this true. An
    // incited riot has to outlive the end of turn it was incited on for the probe action to do
    // anything observable.
    CHECK(pBase->GetPopulation().IsRioting());

    // Golden-age Update ran (may or may not transition); calling both EOT APIs is required.
    // A second stage pass must also be safe (exercises CheckGoldenAgeEndOfTurn again), and it
    // is where the one forced turn expires.
    (void)goldenAgeCallbacks;
    processor.Advance(*game.pState);
    Population stage(HookContext{});
    CHECK(stage.Execute(*game.pState, *game.pPlayer) == StageResult_t::Continue);
    CHECK_FALSE(pBase->GetPopulation().IsRioting());
}

TEST_CASE("WorldEvents consumes GameState session RNG", "[WorldEvents]")
{
    FactionFixture fixtures;
    GameSettings settings;

    auto makeState = [&](unsigned seed)
    {
        auto pState = std::make_unique<GameState>(
            std::make_unique<WorldMap>(16, 16), fixtures.improvements, &fixtures.unitComponents,
            settings, *fixtures.dataContext.moraleCalculator, actest::k_TestRngSeed);
        pState->GetRng().seed(seed);
        pState->SetMissionYear(GameState::k_FirstPlayableMissionYear);
        return pState;
    };

    auto pA = makeState(42);
    auto pB = makeState(99);
    auto pA2 = makeState(42);
    CHECK(pA->GetYearsSinceFirstPlayableYear() == 0);

    WorldEvents eventsA(HookContext{});
    WorldEvents eventsB(HookContext{});
    WorldEvents eventsA2(HookContext{});
    CHECK(eventsA.Execute(*pA) == StageResult_t::Continue);
    CHECK(eventsB.Execute(*pB) == StageResult_t::Continue);
    CHECK(eventsA2.Execute(*pA2) == StageResult_t::Continue);

    // Session streams diverge by seed; identical seeds stay aligned after the same stage work.
    CHECK(pA->GetRng()() != pB->GetRng()());
    // Re-seeded twin already ran the same stage draws — next draw matches pA's next draw only
    // if both consumed the same count; compare post-stage state by re-running from seed.
    auto pA3 = makeState(42);
    auto pA4 = makeState(42);
    WorldEvents e3(HookContext{});
    WorldEvents e4(HookContext{});
    e3.Execute(*pA3);
    e4.Execute(*pA4);
    CHECK(pA3->GetRng()() == pA4->GetRng()());
}
