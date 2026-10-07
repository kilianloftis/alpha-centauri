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
    REQUIRE(game.Owner(8, 8) == game.pA->GetFactionId());
    REQUIRE(game.Owner(9, 9) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 8, 8);

    // Planning routes into the territory; only the step itself is refused.
    REQUIRE(game.pState->GetPathfinder().FindPath(rScout, game.At(10, 10)).bReachable);
    const StepEvaluator steps(game.Map(), game.pState->GetTileEffects());
    CHECK(steps.EvaluateStep(rScout, rScout.GetTile(), game.At(9, 9)).outcome
          == StepOutcome_t::BlockedByTerritory);

    rScout.SetOrder(MoveOrder_t{&game.At(10, 10)});
    UnitOrderExecutor& rOrders = game.pState->GetUnitOrderExecutor();
    CHECK(rOrders.Execute(rScout) == OrderProgress_t::Continue);

    CHECK(&rScout.GetTile() == &game.At(8, 8));
    CHECK(rScout.GetOrder().has_value());
    const std::vector<TerritoryEntryInteraction_t> entries = game.Queued<TerritoryEntryInteraction_t>();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().unitId == rScout.GetUnitId());
    CHECK(entries.front().ownerId == game.pB->GetFactionId());
}

TEST_CASE("A covert unit enters Treaty territory without asking",
          "[diplomacy][status][territory][covert]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    REQUIRE(game.Owner(9, 9) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 8, 8, {"test_chassis", "covert"});

    rScout.SetOrder(MoveOrder_t{&game.At(9, 9)});
    game.pState->GetUnitOrderExecutor().Execute(rScout);

    CHECK(&rScout.GetTile() == &game.At(9, 9));
    CHECK(game.Queued<TerritoryEntryInteraction_t>().empty());
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Breaking the Treaty at the border declares Vendetta and resumes the move",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 8, 8);
    rScout.SetOrder(MoveOrder_t{&game.At(9, 9)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    BreakAgreementAndContinue(*game.pState, rScout, game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(&rScout.GetTile() == &game.At(9, 9));
}

TEST_CASE("An AI unit drops a move order that would enter forbidden territory",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pB, 9, 9);
    rScout.SetOrder(MoveOrder_t{&game.At(7, 7)});

    CHECK(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Complete);
    CHECK_FALSE(rScout.GetOrder().has_value());
    CHECK(&rScout.GetTile() == &game.At(9, 9));
    CHECK(game.Queued<TerritoryEntryInteraction_t>().empty());
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Path planning keeps out of forbidden territory when a route avoids it",
          "[diplomacy][status][territory]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 12, 4);

    const Path_t path = game.pState->GetPathfinder().FindPath(rScout, game.At(4, 12));
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
    Unit& rScout = game.MakeUnit(*game.pA, 8, 8);
    rScout.SetOrder(MoveOrder_t{&game.At(9, 9)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    BreakAgreementAndContinue(*game.pState, rScout, game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
}
