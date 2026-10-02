#include "DiplomacyFixture.h"

#include "game/faction/DiplomaticPermissionRules.h"
#include "game/units/MovementRules.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("Only Treaty keeps a faction's units out of the other's territory",
          "[diplomacy][status][movement]")
{
    DiplomacyFixture game;
    REQUIRE(game.Owner(6, 4) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);

    for (const DiplomaticStatus_t status :
         {DiplomaticStatus_t::Neutral, DiplomaticStatus_t::Pact, DiplomaticStatus_t::Vendetta})
    {
        game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), status);
        CHECK(CanEnterTile(rScout, game.At(6, 4), game.Map(), game.Grids()));
    }

    game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                            DiplomaticStatus_t::Treaty);
    CHECK_FALSE(CanEnterTile(rScout, game.At(6, 4), game.Map(), game.Grids()));
    CHECK(CanEnterTile(rScout, game.At(2, 4), game.Map(), game.Grids()));
}

TEST_CASE("Only Pact partners may stand on the same tile", "[diplomacy][status][movement]")
{
    DiplomacyFixture game;
    Unit& rMover = game.MakeUnit(*game.pA, 3, 0);
    Unit& rOther = game.MakeUnit(*game.pB, 4, 0);

    CHECK_FALSE(HasFriendlyOccupant(rMover, rOther.GetTile(), game.Map()));
    MoveOrder_t order;
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStep(rMover, rOther.GetTile(), order).bEntered);

    game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                            DiplomaticStatus_t::Pact);
    CHECK(HasFriendlyOccupant(rMover, rOther.GetTile(), game.Map()));
    CHECK(HasFriendlyBase(rMover, game.pBaseB->GetTile(), game.Map()));
    CHECK(game.pState->GetUnitOrderExecutor().TryStep(rMover, rOther.GetTile(), order).bEntered);
    CHECK(&rMover.GetTile() == &rOther.GetTile());
}

TEST_CASE("Units repair in their own bases and in Pact partners' bases",
          "[diplomacy][status][repair]")
{
    DiplomacyFixture game;
    Unit& rUnit = game.MakeUnit(*game.pA, 4, 4);

    CHECK(MayRepairAt(rUnit, *game.pBaseA));
    CHECK_FALSE(MayRepairAt(rUnit, *game.pBaseB));

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    CHECK_FALSE(MayRepairAt(rUnit, *game.pBaseB));

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);
    CHECK(MayRepairAt(rUnit, *game.pBaseB));
}
