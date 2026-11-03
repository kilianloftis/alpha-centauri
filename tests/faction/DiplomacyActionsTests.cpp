#include "game/diplomacy/DiplomacyActions.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/diplomacy/TradeItem.h"

#include <catch2/catch_test_macros.hpp>
#include <magic_enum.hpp>

#include <algorithm>
#include <vector>

using namespace ac;

namespace
{

void Meet_(DiplomacyLedger& rLedger)
{
    rLedger.SetKnown(1, 2, true);
}

bool HasKind_(const std::vector<DiplomaticActionKind_t>& rActions, DiplomaticActionKind_t kind)
{
    return std::find(rActions.begin(), rActions.end(), kind) != rActions.end();
}

} // namespace

TEST_CASE("Unknown factions have no available actions", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    CHECK(GetAvailableActions(ledger, 1, 2).empty());
    CHECK_FALSE(CanProposeStepUp(ledger, 1, 2));
}

TEST_CASE("Neutral allows treaty, vendetta, and trade", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    CHECK(CanProposeStepUp(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
    CHECK_FALSE(CanCancelTreaty(ledger, 1, 2));

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::DeclareVendetta));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::Trade));
}

TEST_CASE("Vendetta offers a truce proposal and trade", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Vendetta);

    CHECK(CanProposeStepUp(ledger, 1, 2));
    CHECK_FALSE(CanCancelTreaty(ledger, 1, 2));
    CHECK_FALSE(CanDeclareVendetta(ledger, 1, 2));

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::Trade));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::DeclareVendetta));
}

TEST_CASE("The proposal offered is the next status up", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(HasKind_(GetAvailableActions(ledger, 1, 2), DiplomaticActionKind_t::ProposeTreaty));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(HasKind_(GetAvailableActions(ledger, 1, 2), DiplomaticActionKind_t::ProposePact));
}

TEST_CASE("Every trade kind can be offered, each once", "[diplomacy][actions]")
{
    const std::vector<TradeKind_t> kinds = TradeKinds();
    CHECK(kinds.size() == magic_enum::enum_count<TradeKind_t>());
    for (const TradeKind_t kind : magic_enum::enum_values<TradeKind_t>())
    {
        CHECK(std::count(kinds.begin(), kinds.end(), kind) == 1);
    }
}

TEST_CASE("Talk offers proposals and trade, and cancels only a Pact", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    CHECK(GetTalkActions(ledger, 1, 2).empty());

    Meet_(ledger);
    const auto neutral = GetTalkActions(ledger, 1, 2);
    CHECK(HasKind_(neutral, DiplomaticActionKind_t::ProposeTreaty));
    CHECK(HasKind_(neutral, DiplomaticActionKind_t::Trade));
    CHECK_FALSE(HasKind_(neutral, DiplomaticActionKind_t::DeclareVendetta));

    for (const DiplomaticStatus_t status : {DiplomaticStatus_t::Truce, DiplomaticStatus_t::Treaty})
    {
        ledger.SetStatus(1, 2, status);
        CHECK_FALSE(HasKind_(GetTalkActions(ledger, 1, 2), DiplomaticActionKind_t::CancelTreaty));
        CHECK(HasKind_(GetAvailableActions(ledger, 1, 2), DiplomaticActionKind_t::CancelTreaty));
    }

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    const auto pact = GetTalkActions(ledger, 1, 2);
    CHECK(HasKind_(pact, DiplomaticActionKind_t::CancelTreaty));
    CHECK_FALSE(HasKind_(pact, DiplomaticActionKind_t::DeclareVendetta));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Vendetta);
    CHECK(HasKind_(GetTalkActions(ledger, 1, 2), DiplomaticActionKind_t::ProposeTruce));
}
