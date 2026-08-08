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

} // namespace

bool CanProposeTruce(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    const DiplomaticStatus status = rLedger.GetStatus(a, b);
    return status == DiplomaticStatus::None || status == DiplomaticStatus::Vendetta;
}

bool CanProposeFriendship(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) == DiplomaticStatus::Truce;
}

bool CanProposePact(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) == DiplomaticStatus::Friendship;
}

bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) != DiplomaticStatus::Vendetta;
}

bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!RequireKnown_(rLedger, a, b))
    {
        return false;
    }
    const DiplomaticStatus status = rLedger.GetStatus(a, b);
    return status == DiplomaticStatus::Truce
        || status == DiplomaticStatus::Friendship
        || status == DiplomaticStatus::Pact;
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
    if (rLedger.GetStatus(a, b) == DiplomaticStatus::Vendetta)
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

std::vector<DiplomaticActionKind> GetAvailableActions(const DiplomacyLedger& rLedger,
                                                      FactionId_t a,
                                                      FactionId_t b)
{
    std::vector<DiplomaticActionKind> actions;
    if (CanProposeTruce(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind::ProposeTruce);
    }
    if (CanProposeFriendship(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind::ProposeFriendship);
    }
    if (CanProposePact(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind::ProposePact);
    }
    if (CanDeclareVendetta(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind::DeclareVendetta);
    }
    if (CanCancelTreaty(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind::CancelTreaty);
    }
    if (!GetAvailableTrades(rLedger, a, b).empty())
    {
        actions.push_back(DiplomaticActionKind::Trade);
    }
    return actions;
}

std::string ToString(DiplomaticActionKind kind)
{
    switch (kind)
    {
    case DiplomaticActionKind::ProposeTruce:
        return "Propose Truce";
    case DiplomaticActionKind::ProposeFriendship:
        return "Propose Friendship";
    case DiplomaticActionKind::ProposePact:
        return "Propose Pact";
    case DiplomaticActionKind::DeclareVendetta:
        return "Declare Vendetta";
    case DiplomaticActionKind::CancelTreaty:
        return "Cancel Treaty";
    case DiplomaticActionKind::Trade:
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
