#include "game/diplomacy/DiplomacyActions.h"
#include "game/diplomacy/DiplomacyLedger.h"
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

TEST_CASE("Factions that have not met have nothing to say", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    CHECK(GetTalkActions(ledger, 1, 2).empty());
}

TEST_CASE("Neutral talk offers a Treaty and trade", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    const auto actions = GetTalkActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::RenouncePact));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::Trade));
}

TEST_CASE("Vendetta talk offers a Truce and trade", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Vendetta);

    const auto actions = GetTalkActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::Trade));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
}

TEST_CASE("The proposal offered is the next status up", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(HasKind_(GetTalkActions(ledger, 1, 2), DiplomaticActionKind_t::ProposeTreaty));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(HasKind_(GetTalkActions(ledger, 1, 2), DiplomaticActionKind_t::ProposePact));
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

TEST_CASE("Only a Pact can be renounced in talk", "[diplomacy][actions][talk]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    for (const DiplomaticStatus_t status : {DiplomaticStatus_t::Neutral, DiplomaticStatus_t::Truce,
                                            DiplomaticStatus_t::Treaty,
                                            DiplomaticStatus_t::Vendetta})
    {
        ledger.SetStatus(1, 2, status);
        CHECK_FALSE(HasKind_(GetTalkActions(ledger, 1, 2), DiplomaticActionKind_t::RenouncePact));
    }

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    const auto pact = GetTalkActions(ledger, 1, 2);
    CHECK(HasKind_(pact, DiplomaticActionKind_t::RenouncePact));
    CHECK(HasKind_(pact, DiplomaticActionKind_t::Trade));
}
