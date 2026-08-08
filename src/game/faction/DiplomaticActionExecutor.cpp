#include "game/faction/DiplomaticActionExecutor.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyActions.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace ac
{

namespace
{

Faction* FindFaction_(GameState& rState, FactionId_t id)
{
    for (Faction& rFaction : rState.Factions())
    {
        if (rFaction.GetFactionId() == id)
        {
            return &rFaction;
        }
    }
    return nullptr;
}

const Faction* FindFaction_(const GameState& rState, FactionId_t id)
{
    for (const Faction& rFaction : rState.Factions())
    {
        if (rFaction.GetFactionId() == id)
        {
            return &rFaction;
        }
    }
    return nullptr;
}

bool StatusRequestLegal_(const DiplomacyLedger& rLedger,
                         FactionId_t a,
                         FactionId_t b,
                         DiplomaticStatus_t requested)
{
    switch (requested)
    {
    case DiplomaticStatus_t::Truce:
        return CanProposeTruce(rLedger, a, b);
    case DiplomaticStatus_t::Friendship:
        return CanProposeFriendship(rLedger, a, b);
    case DiplomaticStatus_t::Pact:
        return CanProposePact(rLedger, a, b);
    case DiplomaticStatus_t::Vendetta:
        return CanDeclareVendetta(rLedger, a, b);
    case DiplomaticStatus_t::None:
        return CanCancelTreaty(rLedger, a, b);
    }
    return false;
}

BaseManager* FindOwnedBase_(Faction& rFaction, BaseId_t baseId)
{
    for (BaseManager& rBase : rFaction.Bases())
    {
        if (rBase.GetBaseId() == baseId)
        {
            return &rBase;
        }
    }
    return nullptr;
}

const BaseManager* FindOwnedBase_(const Faction& rFaction, BaseId_t baseId)
{
    for (const BaseManager& rBase : rFaction.Bases())
    {
        if (rBase.GetBaseId() == baseId)
        {
            return &rBase;
        }
    }
    return nullptr;
}

} // namespace

DiplomaticProposeResult_t DiplomaticActionExecutor::Propose(GameState& rState,
                                                          const DiplomaticProposal_t& rProposal)
{
    if (!Validate_(rState, rProposal))
    {
        return DiplomaticProposeResult_t::Invalid;
    }

    Faction* pRecipient = FindFaction_(rState, rProposal.recipient);
    if (!pRecipient)
    {
        return DiplomaticProposeResult_t::Invalid;
    }

    if (pRecipient->IsPlayerControlled())
    {
        // One slot: overwriting it stranded the earlier proposer, which had already been told
        // to wait. A per-recipient queue needs ordering/expiry rules that are not written down
        // anywhere yet, so refuse rather than lose the proposal.
        if (m_pending.has_value())
        {
            return DiplomaticProposeResult_t::Busy;
        }
        m_pending = rProposal;
        return DiplomaticProposeResult_t::PendingPlayer;
    }

    if (!EvaluateResponse_(rState, rProposal.recipient))
    {
        return DiplomaticProposeResult_t::Rejected;
    }

    Apply_(rState, rProposal);
    return DiplomaticProposeResult_t::Accepted;
}

bool DiplomaticActionExecutor::Accept(GameState& rState)
{
    if (!m_pending.has_value())
    {
        return false;
    }
    DiplomaticProposal_t proposal = *m_pending;
    m_pending.reset();
    if (!Validate_(rState, proposal))
    {
        return false;
    }
    Apply_(rState, proposal);
    return true;
}

void DiplomaticActionExecutor::Reject()
{
    m_pending.reset();
}

bool DiplomaticActionExecutor::Validate_(GameState& rState,
                                         const DiplomaticProposal_t& rProposal) const
{
    if (rProposal.proposer == rProposal.recipient)
    {
        return false;
    }
    if (!FindFaction_(rState, rProposal.proposer) || !FindFaction_(rState, rProposal.recipient))
    {
        return false;
    }

    const DiplomacyLedger& rLedger = rState.GetDiplomacyLedger();
    if (!rLedger.AreKnown(rProposal.proposer, rProposal.recipient))
    {
        return false;
    }

    if (rProposal.requestedStatus.has_value())
    {
        if (!StatusRequestLegal_(rLedger, rProposal.proposer, rProposal.recipient,
                                 *rProposal.requestedStatus))
        {
            return false;
        }
    }

    for (const TradeItem_t& rItem : rProposal.give)
    {
        if (!CanTrade(rLedger, rProposal.proposer, rProposal.recipient, rItem))
        {
            return false;
        }
    }
    for (const TradeItem_t& rItem : rProposal.demand)
    {
        if (!CanTrade(rLedger, rProposal.proposer, rProposal.recipient, rItem))
        {
            return false;
        }
    }

    if (!ValidateItems_(rState, rProposal.proposer, rProposal.recipient, rProposal.give))
    {
        return false;
    }
    if (!ValidateItems_(rState, rProposal.recipient, rProposal.proposer, rProposal.demand))
    {
        return false;
    }

    if (!ValidateGiverTotals_(rState, rProposal.proposer, rProposal.give))
    {
        return false;
    }
    if (!ValidateGiverTotals_(rState, rProposal.recipient, rProposal.demand))
    {
        return false;
    }
    return true;
}

bool DiplomaticActionExecutor::ValidateGiverTotals_(GameState& rState,
                                                    FactionId_t giverId,
                                                    const std::vector<TradeItem_t>& rItems) const
{
    const Faction* pGiver = FindFaction_(rState, giverId);
    if (!pGiver)
    {
        return false;
    }

    GiverCost_t cost;
    for (const TradeItem_t& rItem : rItems)
    {
        if (const auto* pCredits = std::get_if<TradeCredits_t>(&rItem))
        {
            cost.credits += pCredits->amount;
        }
        else if (const auto* pBase = std::get_if<TradeBase_t>(&rItem))
        {
            if (std::find(cost.baseIds.begin(), cost.baseIds.end(), pBase->baseId)
                != cost.baseIds.end())
            {
                return false;
            }
            cost.baseIds.push_back(pBase->baseId);
        }
    }

    return pGiver->GetEconomy().CanAfford(cost.credits);
}

bool DiplomaticActionExecutor::ValidateItems_(GameState& rState,
                                              FactionId_t giverId,
                                              FactionId_t receiverId,
                                              const std::vector<TradeItem_t>& rItems) const
{
    for (const TradeItem_t& rItem : rItems)
    {
        if (!ValidateItem_(rState, giverId, receiverId, rItem))
        {
            return false;
        }
    }
    return true;
}

bool DiplomaticActionExecutor::ValidateItem_(GameState& rState,
                                             FactionId_t giverId,
                                             FactionId_t receiverId,
                                             const TradeItem_t& rItem) const
{
    const Faction* pGiver = FindFaction_(rState, giverId);
    if (!pGiver)
    {
        return false;
    }

    return std::visit(
        [&](const auto& rConcrete) -> bool
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, TradeCredits_t>)
            {
                return rConcrete.amount > 0 && pGiver->GetEconomy().GetEnergy() >= rConcrete.amount;
            }
            else if constexpr (std::is_same_v<T, TradeTechnology_t>)
            {
                return pGiver->GetResearch().HasDiscoveredTech(rConcrete.techId)
                    && FindFaction_(rState, receiverId)
                    && !FindFaction_(rState, receiverId)->GetResearch().HasDiscoveredTech(
                           rConcrete.techId);
            }
            else if constexpr (std::is_same_v<T, TradeBase_t>)
            {
                return FindOwnedBase_(*pGiver, rConcrete.baseId) != nullptr;
            }
            else if constexpr (std::is_same_v<T, TradeCommFrequency_t>)
            {
                // Giver must already know the introduced faction; receiver must not yet.
                const DiplomacyLedger& rLedger = rState.GetDiplomacyLedger();
                return rConcrete.factionId != giverId
                    && rConcrete.factionId != receiverId
                    && FindFaction_(rState, rConcrete.factionId) != nullptr
                    && rLedger.AreKnown(giverId, rConcrete.factionId)
                    && !rLedger.AreKnown(receiverId, rConcrete.factionId);
            }
            else if constexpr (std::is_same_v<T, TradeWorldMap_t>)
            {
                return pGiver->GetExploredMap().IsSized();
            }
            else if constexpr (std::is_same_v<T, TradeDeclareVendetta_t>)
            {
                const DiplomacyLedger& rLedger = rState.GetDiplomacyLedger();
                return rConcrete.againstFactionId != giverId
                    && rConcrete.againstFactionId != receiverId
                    && FindFaction_(rState, rConcrete.againstFactionId) != nullptr
                    && rLedger.AreKnown(receiverId, rConcrete.againstFactionId)
                    && !rLedger.HasVendetta(receiverId, rConcrete.againstFactionId);
            }
            return false;
        },
        rItem);
}

bool DiplomaticActionExecutor::EvaluateResponse_(GameState& /*rState*/,
                                                 FactionId_t /*recipientId*/) const
{
    // AI stub: always agree.
    return true;
}

void DiplomaticActionExecutor::Apply_(GameState& rState, const DiplomaticProposal_t& rProposal)
{
    if (rProposal.requestedStatus.has_value())
    {
        rState.GetDiplomacyLedger().SetStatus(
            rProposal.proposer, rProposal.recipient, *rProposal.requestedStatus);
    }
    ApplyItems_(rState, rProposal.proposer, rProposal.recipient, rProposal.give);
    ApplyItems_(rState, rProposal.recipient, rProposal.proposer, rProposal.demand);
}

void DiplomaticActionExecutor::ApplyItems_(GameState& rState,
                                           FactionId_t giverId,
                                           FactionId_t receiverId,
                                           const std::vector<TradeItem_t>& rItems)
{
    for (const TradeItem_t& rItem : rItems)
    {
        ApplyItem_(rState, giverId, receiverId, rItem);
    }
}

void DiplomaticActionExecutor::ApplyItem_(GameState& rState,
                                          FactionId_t giverId,
                                          FactionId_t receiverId,
                                          const TradeItem_t& rItem)
{
    Faction* pGiver = FindFaction_(rState, giverId);
    Faction* pReceiver = FindFaction_(rState, receiverId);
    if (!pGiver || !pReceiver)
    {
        throw std::runtime_error("DiplomaticActionExecutor::ApplyItem_: missing faction");
    }

    std::visit(
        [&](const auto& rConcrete)
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, TradeCredits_t>)
            {
                pGiver->GetEconomy().SpendEnergy(rConcrete.amount);
                pReceiver->GetEconomy().AddEnergy(rConcrete.amount);
            }
            else if constexpr (std::is_same_v<T, TradeTechnology_t>)
            {
                pReceiver->GetResearch().AddDiscoveredTech(rConcrete.techId);
            }
            else if constexpr (std::is_same_v<T, TradeBase_t>)
            {
                pGiver->TransferBaseTo(rConcrete.baseId, *pReceiver);
            }
            else if constexpr (std::is_same_v<T, TradeCommFrequency_t>)
            {
                rState.GetDiplomacyLedger().SetKnown(receiverId, rConcrete.factionId);
            }
            else if constexpr (std::is_same_v<T, TradeWorldMap_t>)
            {
                pReceiver->GetExploredMap().MergeFrom(pGiver->GetExploredMap());
            }
            else if constexpr (std::is_same_v<T, TradeDeclareVendetta_t>)
            {
                rState.GetDiplomacyLedger().SetStatus(
                    receiverId, rConcrete.againstFactionId, DiplomaticStatus_t::Vendetta);
            }
        },
        rItem);
}

} // namespace ac
