#include "game/faction/DiplomacyActions.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/TradeItem.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace ac;

namespace
{

void Meet_(DiplomacyLedger& rLedger)
{
    rLedger.SetKnown(1, 2);
}

bool HasKind_(const std::vector<DiplomaticActionKind_t>& rActions, DiplomaticActionKind_t kind)
{
    return std::find(rActions.begin(), rActions.end(), kind) != rActions.end();
}

bool HasTrade_(const std::vector<TradeKind_t>& rKinds, TradeKind_t kind)
{
    return std::find(rKinds.begin(), rKinds.end(), kind) != rKinds.end();
}

} // namespace

TEST_CASE("Unknown factions have no available actions", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    CHECK(GetAvailableActions(ledger, 1, 2).empty());
    CHECK(GetAvailableTrades(ledger, 1, 2).empty());
    CHECK_FALSE(CanProposeTruce(ledger, 1, 2));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
}

TEST_CASE("None status allows truce, vendetta, and ordinary trade", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    CHECK(CanProposeTruce(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
    CHECK(CanTrade(ledger, 1, 2, TradeCredits_t{10}));
    CHECK(CanTrade(ledger, 1, 2, TradeTechnology_t{"tech"}));
    CHECK(CanTrade(ledger, 1, 2, TradeWorldMap_t{}));
    CHECK(CanTrade(ledger, 1, 2, TradeCommFrequency_t{3}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeDeclareVendetta_t{3}));
    CHECK_FALSE(CanProposeFriendship(ledger, 1, 2));
    CHECK_FALSE(CanCancelTreaty(ledger, 1, 2));

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::DeclareVendetta));
    CHECK(HasKind_(actions, DiplomaticActionKind_t::Trade));

    const auto trades = GetAvailableTrades(ledger, 1, 2);
    CHECK(HasTrade_(trades, TradeKind_t::Credits));
    CHECK(HasTrade_(trades, TradeKind_t::Technology));
    CHECK(HasTrade_(trades, TradeKind_t::CommFrequency));
    CHECK(HasTrade_(trades, TradeKind_t::WorldMap));
    CHECK_FALSE(HasTrade_(trades, TradeKind_t::Base));
    CHECK_FALSE(HasTrade_(trades, TradeKind_t::DeclareVendetta));
}

TEST_CASE("Vendetta blocks trade and allows only truce", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Vendetta);

    CHECK(CanProposeTruce(ledger, 1, 2));
    CHECK_FALSE(CanDeclareVendetta(ledger, 1, 2));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK(GetAvailableTrades(ledger, 1, 2).empty());

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::Trade));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::DeclareVendetta));
}

TEST_CASE("Bases and coordinated vendetta require Pact", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Friendship);
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeDeclareVendetta_t{3}));
    CHECK(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
    {
        const auto trades = GetAvailableTrades(ledger, 1, 2);
        CHECK_FALSE(HasTrade_(trades, TradeKind_t::Base));
        CHECK_FALSE(HasTrade_(trades, TradeKind_t::DeclareVendetta));
        CHECK(HasTrade_(trades, TradeKind_t::Credits));
    }

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    CHECK(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK(CanTrade(ledger, 1, 2, TradeDeclareVendetta_t{3}));
    CHECK(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
    {
        const auto trades = GetAvailableTrades(ledger, 1, 2);
        CHECK(HasTrade_(trades, TradeKind_t::Base));
        CHECK(HasTrade_(trades, TradeKind_t::DeclareVendetta));
        CHECK(trades.size() == 6);
    }
}

TEST_CASE("Treaty ladder Friendship and Pact", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(CanProposeFriendship(ledger, 1, 2));
    CHECK(CanCancelTreaty(ledger, 1, 2));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Friendship);
    CHECK(CanProposePact(ledger, 1, 2));
    CHECK(CanCancelTreaty(ledger, 1, 2));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    CHECK_FALSE(CanProposePact(ledger, 1, 2));
    CHECK(CanCancelTreaty(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
}

TEST_CASE("DiplomaticActionKind_t and TradeKind_t ToString are non-empty", "[diplomacy][actions]")
{
    CHECK_FALSE(ToString(DiplomaticActionKind_t::ProposeTruce).empty());
    CHECK_FALSE(ToString(DiplomaticActionKind_t::Trade).empty());
    CHECK_FALSE(ToString(TradeKind_t::Credits).empty());
    CHECK_FALSE(ToString(TradeKind_t::DeclareVendetta).empty());
}
