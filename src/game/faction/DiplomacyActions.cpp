#include "game/faction/DiplomacyActions.h"

#include <cstddef>
#include <type_traits>
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

std::vector<TradeKind> GetAvailableTrades(const DiplomacyLedger& rLedger,
                                          FactionId_t a,
                                          FactionId_t b)
{
    std::vector<TradeKind> kinds;
    // Probe each category with a dummy payload; CanTrade only gates on relationship.
    const TradeItem_t probes[] = {
        TradeCredits_t{1},
        TradeTechnology_t{},
        TradeBase_t{1},
        TradeCommFrequency_t{0},
        TradeWorldMap_t{},
        TradeDeclareVendetta_t{0},
    };
    const TradeKind kindOrder[] = {
        TradeKind::Credits,
        TradeKind::Technology,
        TradeKind::Base,
        TradeKind::CommFrequency,
        TradeKind::WorldMap,
        TradeKind::DeclareVendetta,
    };
    for (size_t i = 0; i < std::size(probes); ++i)
    {
        if (CanTrade(rLedger, a, b, probes[i]))
        {
            kinds.push_back(kindOrder[i]);
        }
    }
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

std::string ToString(TradeKind kind)
{
    switch (kind)
    {
    case TradeKind::Credits:
        return "Credits";
    case TradeKind::Technology:
        return "Technology";
    case TradeKind::Base:
        return "Base";
    case TradeKind::CommFrequency:
        return "Comm Frequency";
    case TradeKind::WorldMap:
        return "World Map";
    case TradeKind::DeclareVendetta:
        return "Declare Vendetta";
    }
    return "Unknown";
}

} // namespace ac
