#include "DiplomacyFixture.h"

#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

namespace
{

void Meet_(DiplomacyLedger& rLedger)
{
    rLedger.SetKnown(1, 2);
}

} // namespace

TEST_CASE("Proposals step up one status at a time", "[diplomacy][transitions]")
{
    CHECK(StepUp(DiplomaticStatus_t::Neutral) == DiplomaticStatus_t::Treaty);
    CHECK(StepUp(DiplomaticStatus_t::Truce) == DiplomaticStatus_t::Treaty);
    CHECK(StepUp(DiplomaticStatus_t::Treaty) == DiplomaticStatus_t::Pact);
    CHECK(StepUp(DiplomaticStatus_t::Vendetta) == DiplomaticStatus_t::Truce);
    CHECK_FALSE(StepUp(DiplomaticStatus_t::Pact).has_value());

    DiplomacyLedger ledger;
    Meet_(ledger);
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    CHECK_FALSE(CanProposeStepUp(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
}

TEST_CASE("Canceling steps down one status at a time", "[diplomacy][transitions]")
{
    CHECK(StepDown(DiplomaticStatus_t::Pact) == DiplomaticStatus_t::Treaty);
    CHECK(StepDown(DiplomaticStatus_t::Treaty) == DiplomaticStatus_t::Neutral);
    CHECK(StepDown(DiplomaticStatus_t::Truce) == DiplomaticStatus_t::Neutral);
    CHECK_FALSE(StepDown(DiplomaticStatus_t::Neutral).has_value());
    CHECK_FALSE(StepDown(DiplomaticStatus_t::Vendetta).has_value());

    DiplomacyLedger ledger;
    Meet_(ledger);
    for (const DiplomaticStatus_t status :
         {DiplomaticStatus_t::Truce, DiplomaticStatus_t::Treaty, DiplomaticStatus_t::Pact})
    {
        ledger.SetStatus(1, 2, status);
        CHECK(CanCancelTreaty(ledger, 1, 2));
    }
}

TEST_CASE("An attributed act the pair's status forbids is a sneak attack",
          "[diplomacy][transitions][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    const HostileAct_t attributed{.aggressor = game.pA->GetFactionId(),
                                  .victim = game.pB->GetFactionId(),
                                  .bAttributed = true};
    HostileAct_t covert = attributed;
    covert.bAttributed = false;

    CHECK(VendettaFromHostileAct(attributed) == VendettaKind_t::SneakAttack);
    CHECK_FALSE(VendettaFromHostileAct(covert).has_value());
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    CHECK_FALSE(VendettaFromHostileAct(attributed).has_value());
}

TEST_CASE("Only a declared Vendetta withdraws units from each other's territory",
          "[diplomacy][transitions][vendetta]")
{
    CHECK(VendettaWithdrawsTerritory(VendettaKind_t::Declaration));
    CHECK_FALSE(VendettaWithdrawsTerritory(VendettaKind_t::SneakAttack));
}

TEST_CASE("An honored obligation cascades only under SeparateDeclaration",
          "[diplomacy][transitions][obligation]")
{
    DiplomacyFixture game;
    DiplomacyConfig_t& rConfig = *game.fixtures.dataContext.diplomacyConfig;
    rConfig.defensiveObligationMode = DefensiveObligationMode_t::JoinAsDefender;
    CHECK_FALSE(HonoredObligationCascades());
    rConfig.defensiveObligationMode = DefensiveObligationMode_t::SeparateDeclaration;
    CHECK(HonoredObligationCascades());
}

TEST_CASE("A bombard offends each struck owner once, or an empty improved tile's territory owner",
          "[diplomacy][transitions][hostile][bombard]")
{
    DiplomacyFixture game;
    Unit& rAttacker = game.MakeUnit(*game.pA, 12, 4, {"test_chassis", "bombard"});
    game.MakeUnit(*game.pB, 13, 5, {"test_chassis", "covert"});
    game.MakeUnit(*game.pB, 13, 5);
    const TileEffectsContext& rTileEffects = game.pState->GetTileEffects();

    const std::vector<HostileAct_t> struck =
        BombardHostileActs(rAttacker, game.At(13, 5), game.Map(), rTileEffects);
    REQUIRE(struck.size() == 1);
    CHECK(struck.front().victim == game.pB->GetFactionId());
    CHECK(struck.front().bAttributed);

    Tile& rFarm = game.At(11, 9);
    REQUIRE(game.Owner(11, 9) == game.pB->GetFactionId());
    rFarm.AddImprovement(game.fixtures.improvements.Get("Farm"));
    const std::vector<HostileAct_t> wrecked =
        BombardHostileActs(rAttacker, rFarm, game.Map(), rTileEffects);
    REQUIRE(wrecked.size() == 1);
    CHECK(wrecked.front().victim == game.pB->GetFactionId());
}
