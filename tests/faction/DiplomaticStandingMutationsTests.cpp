#include "DiplomacyFixture.h"

#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("ApplyFriction adds and clamps, directed", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();

    ApplyFriction(game.fixtures.state, a, b, 19);
    CHECK(game.Ledger().GetFriction(a, b) == 19);
    CHECK(game.Ledger().GetFriction(b, a) == 0);

    ApplyFriction(game.fixtures.state, a, b, 5);
    CHECK(game.Ledger().GetFriction(a, b) == 20);

    ApplyFriction(game.fixtures.state, a, b, -100);
    CHECK(game.Ledger().GetFriction(a, b) == 0);
}

TEST_CASE("ApplyBlemishes adds and clamps", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 98});
    CHECK(game.Ledger().GetBlemishes(a) == 98);

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 5});
    CHECK(game.Ledger().GetBlemishes(a) == 99);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("ApplyBlemishes raises to the floor then clamps", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 2, .floor = 6});
    CHECK(game.Ledger().GetBlemishes(a) == 6);

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 2, .floor = 150});
    CHECK(game.Ledger().GetBlemishes(a) == 99);
}

TEST_CASE("Cancelling an agreement blemishes the canceller", "[diplomacy][standing][cancel]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    CancelTreaty(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);

    CancelTreaty(game.fixtures.state, game.pB->GetFactionId(), game.pA->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 1);
}

TEST_CASE("A Neutral attack is not an agreement break", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("A sneak attack blemishes the aggressor", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("An unattributed hostile act adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = false});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Fighting at Vendetta adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("A declared Vendetta from Neutral adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Declaring Vendetta from Treaty blemishes the declarer", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("Declaring Vendetta from Pact blemishes the declarer", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
}

TEST_CASE("An atrocity Vendetta from Treaty adds no agreement-break blemish",
          "[diplomacy][standing][atrocity]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyAtrocityVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Honoring a defensive obligation adds no blemishes",
          "[diplomacy][standing][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pC, *game.pA, DiplomaticStatus_t::Treaty);

    HonorDefensiveObligation(game.fixtures.state, game.pC->GetFactionId(), game.pA->GetFactionId(),
                             VendettaKind_t::Declaration);

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pC->GetFactionId()) == 0);
}

TEST_CASE("A sneak against native life adds no blemishes", "[diplomacy][standing][native]")
{
    DiplomacyFixture game;
    Faction& rPlanet = AddNativeLifeFaction(game.fixtures);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = rPlanet.GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}
