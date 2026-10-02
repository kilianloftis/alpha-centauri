#include "game/faction/DiplomacyActions.h"

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace ac
{

namespace
{

bool RequireKnown_(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    return rLedger.AreKnown(a, b);
}

DiplomaticActionKind_t ProposeKindFor_(DiplomaticStatus_t target)
{
    switch (target)
    {
    case DiplomaticStatus_t::Truce:
        return DiplomaticActionKind_t::ProposeTruce;
    case DiplomaticStatus_t::Treaty:
        return DiplomaticActionKind_t::ProposeTreaty;
    case DiplomaticStatus_t::Pact:
        return DiplomaticActionKind_t::ProposePact;
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Vendetta:
        break;
    }
    throw std::logic_error("ProposeKindFor_: no proposal leads to this status");
}

} // namespace

bool CanProposeStepUp(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    return RequireKnown_(rLedger, a, b) && StepUp(rLedger.GetStatus(a, b)).has_value();
}

bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) != DiplomaticStatus_t::Vendetta;
}

bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    return StepDown(rLedger.GetStatus(a, b)).has_value();
}

bool CanTrade(const DiplomacyLedger& rLedger,
              FactionId_t a,
              FactionId_t b,
              const TradeItem_t& rItem)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    if (rLedger.GetStatus(a, b) == DiplomaticStatus_t::Vendetta)
    {
        return false;
    }

    return std::visit(
        [&](const auto& rConcrete) -> bool
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, TradeBase_t>
                          || std::is_same_v<T, TradeDeclareVendetta_t>)
            {
                return rLedger.HasPact(a, b);
            }
            else
            {
                return true;
            }
        },
        rItem);
}

std::vector<TradeKind_t> GetAvailableTrades(const DiplomacyLedger& rLedger,
                                            FactionId_t a,
                                            FactionId_t b)
{
    // Enumerate the variant itself rather than a hand-kept parallel table: a default-constructed
    // alternative is enough because CanTrade gates on the relationship and the item's *type*,
    // never on payload values.
    std::vector<TradeKind_t> kinds;
    [&]<size_t... I>(std::index_sequence<I...>)
    {
        (([&]
        {
            using Alternative = std::variant_alternative_t<I, TradeItem_t>;
            if (CanTrade(rLedger, a, b, TradeItem_t{Alternative{}}))
            {
                kinds.push_back(TradeKindOf<Alternative>::value);
            }
        }()), ...);
    }(std::make_index_sequence<std::variant_size_v<TradeItem_t>>{});
    return kinds;
}

std::vector<DiplomaticActionKind_t> GetAvailableActions(const DiplomacyLedger& rLedger,
                                                      FactionId_t a,
                                                      FactionId_t b)
{
    std::vector<DiplomaticActionKind_t> actions;
    if (CanProposeStepUp(rLedger, a, b))
    {
        actions.push_back(ProposeKindFor_(*StepUp(rLedger.GetStatus(a, b))));
    }
    if (CanDeclareVendetta(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind_t::DeclareVendetta);
    }
    if (CanCancelTreaty(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind_t::CancelTreaty);
    }
    if (!GetAvailableTrades(rLedger, a, b).empty())
    {
        actions.push_back(DiplomaticActionKind_t::Trade);
    }
    return actions;
}

std::string ToString(DiplomaticActionKind_t kind)
{
    switch (kind)
    {
    case DiplomaticActionKind_t::ProposeTruce:
        return "Propose Truce";
    case DiplomaticActionKind_t::ProposeTreaty:
        return "Propose Treaty";
    case DiplomaticActionKind_t::ProposePact:
        return "Propose Pact";
    case DiplomaticActionKind_t::DeclareVendetta:
        return "Declare Vendetta";
    case DiplomaticActionKind_t::CancelTreaty:
        return "Cancel Treaty";
    case DiplomaticActionKind_t::Trade:
        return "Trade";
    }
    return "Unknown";
}

TradeKind_t KindOf(const TradeItem_t& rItem)
{
    return std::visit(
        [](const auto& rConcrete) { return TradeKindOf<std::decay_t<decltype(rConcrete)>>::value; },
        rItem);
}

std::string ToString(TradeKind_t kind)
{
    // Display labels insert spaces the enumerator names do not carry.
    switch (kind)
    {
    case TradeKind_t::Credits:
        return "Credits";
    case TradeKind_t::Technology:
        return "Technology";
    case TradeKind_t::Base:
        return "Base";
    case TradeKind_t::CommFrequency:
        return "Comm Frequency";
    case TradeKind_t::WorldMap:
        return "World Map";
    case TradeKind_t::DeclareVendetta:
        return "Declare Vendetta";
    }
    throw std::runtime_error("ToString: unhandled TradeKind_t");
}

} // namespace ac
