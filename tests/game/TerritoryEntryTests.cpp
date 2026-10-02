#include "DiplomacyFixture.h"

#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/TerritoryEntryEffects.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;
using namespace actest;

TEST_CASE("A player's move stops at the border of Treaty territory and asks",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    REQUIRE(game.Owner(4, 4) == game.pA->GetFactionId());
    REQUIRE(game.Owner(5, 4) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);

    // Planning routes into the territory; only the step itself is refused.
    REQUIRE(game.pState->GetPathfinder().FindPath(rScout, game.At(6, 4)).bReachable);
    const StepEvaluator steps(game.Map(), game.pState->GetTileEffects());
    CHECK(steps.EvaluateStep(rScout, rScout.GetTile(), game.At(5, 4)).outcome
          == StepOutcome_t::BlockedByTerritory);

    rScout.SetOrder(MoveOrder_t{&game.At(6, 4)});
    UnitOrderExecutor& rOrders = game.pState->GetUnitOrderExecutor();
    CHECK(rOrders.Execute(rScout) == OrderProgress_t::Continue);

    CHECK(&rScout.GetTile() == &game.At(4, 4));
    CHECK(rScout.GetOrder().has_value());
    const std::vector<TerritoryEntryInteraction_t> entries = game.Queued<TerritoryEntryInteraction_t>();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().unitId == rScout.GetUnitId());
    CHECK(entries.front().ownerId == game.pB->GetFactionId());
}

TEST_CASE("Breaking the Treaty at the border declares Vendetta and resumes the move",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), true);

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(&rScout.GetTile() == &game.At(5, 4));
}

TEST_CASE("Cancelling at the border drops the order and keeps the Treaty",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), false);

    CHECK_FALSE(rScout.GetOrder().has_value());
    CHECK(&rScout.GetTile() == &game.At(4, 4));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("An AI unit drops a move order that would enter forbidden territory",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pB, 5, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(3, 4)});

    CHECK(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Complete);
    CHECK_FALSE(rScout.GetOrder().has_value());
    CHECK(&rScout.GetTile() == &game.At(5, 4));
    CHECK(game.Queued<TerritoryEntryInteraction_t>().empty());
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Path planning keeps out of forbidden territory when a route avoids it",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 0);

    const Path_t path = game.pState->GetPathfinder().FindPath(rScout, game.At(4, 8));
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK(game.Map().GetTerritory().GetOwner(*pTile) != game.pB->GetFactionId());
    }
}

TEST_CASE("Breaking a Treaty at the border obliges the owner's Pact partners",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), true);

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
}
