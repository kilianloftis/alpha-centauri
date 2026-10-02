#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticTransitionRules.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

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
