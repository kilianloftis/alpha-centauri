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
    CHECK_FALSE(CanProposeStepUp(ledger, 1, 2));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
}

TEST_CASE("Neutral allows treaty, vendetta, and ordinary trade", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    CHECK(CanProposeStepUp(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
    CHECK(CanTrade(ledger, 1, 2, TradeCredits_t{10}));
    CHECK(CanTrade(ledger, 1, 2, TradeTechnology_t{"tech"}));
    CHECK(CanTrade(ledger, 1, 2, TradeWorldMap_t{}));
    CHECK(CanTrade(ledger, 1, 2, TradeCommFrequency_t{3}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeDeclareVendetta_t{3}));
    CHECK_FALSE(CanCancelTreaty(ledger, 1, 2));

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
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

    CHECK(CanProposeStepUp(ledger, 1, 2));
    CHECK_FALSE(CanCancelTreaty(ledger, 1, 2));
    CHECK_FALSE(CanDeclareVendetta(ledger, 1, 2));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeCredits_t{1}));
    CHECK_FALSE(CanTrade(ledger, 1, 2, TradeBase_t{1}));
    CHECK(GetAvailableTrades(ledger, 1, 2).empty());

    const auto actions = GetAvailableActions(ledger, 1, 2);
    CHECK(HasKind_(actions, DiplomaticActionKind_t::ProposeTruce));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::ProposeTreaty));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::Trade));
    CHECK_FALSE(HasKind_(actions, DiplomaticActionKind_t::DeclareVendetta));
}

TEST_CASE("Bases and coordinated vendetta require Pact", "[diplomacy][actions]")
{
    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
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

TEST_CASE("Proposals step up one status at a time", "[diplomacy][actions]")
{
    CHECK(StepUp(DiplomaticStatus_t::Neutral) == DiplomaticStatus_t::Treaty);
    CHECK(StepUp(DiplomaticStatus_t::Truce) == DiplomaticStatus_t::Treaty);
    CHECK(StepUp(DiplomaticStatus_t::Treaty) == DiplomaticStatus_t::Pact);
    CHECK(StepUp(DiplomaticStatus_t::Vendetta) == DiplomaticStatus_t::Truce);
    CHECK_FALSE(StepUp(DiplomaticStatus_t::Pact).has_value());

    DiplomacyLedger ledger;
    Meet_(ledger);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(HasKind_(GetAvailableActions(ledger, 1, 2), DiplomaticActionKind_t::ProposeTreaty));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(HasKind_(GetAvailableActions(ledger, 1, 2), DiplomaticActionKind_t::ProposePact));

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    CHECK_FALSE(CanProposeStepUp(ledger, 1, 2));
    CHECK(CanDeclareVendetta(ledger, 1, 2));
}

TEST_CASE("Canceling steps down one status at a time", "[diplomacy][actions]")
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

TEST_CASE("DiplomaticActionKind_t and TradeKind_t ToString are non-empty", "[diplomacy][actions]")
{
    CHECK_FALSE(ToString(DiplomaticActionKind_t::ProposeTruce).empty());
    CHECK_FALSE(ToString(DiplomaticActionKind_t::Trade).empty());
    CHECK_FALSE(ToString(TradeKind_t::Credits).empty());
    CHECK_FALSE(ToString(TradeKind_t::DeclareVendetta).empty());
}
