#include "game/diplomacy/DiplomaticProposalRules.h"

#include "game/CurrentGameState.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/economy/EconomyManager.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/research/ResearchManager.h"

#include <algorithm>
#include <set>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

// Whether rGiver can hand the item over to rReceiver.
struct CanDeliver_
{
    const GameState& rState;
    const Faction& rGiver;
    const Faction& rReceiver;

    bool operator()(const TradeCredits_t& rCredits) const
    {
        return rCredits.amount > 0 && rGiver.GetEconomy().CanAfford(rCredits.amount);
    }

    bool operator()(const TradeTechnology_t& rTechnology) const
    {
        return rGiver.GetResearch().HasDiscoveredTech(rTechnology.techId)
            && !rReceiver.GetResearch().HasDiscoveredTech(rTechnology.techId);
    }

    // make_gift and base_swap never take a faction's last base (terranx-map.md, Base swap).
    bool operator()(const TradeBase_t& rBase) const
    {
        return rGiver.FindBase(rBase.baseId) != nullptr && rGiver.GetBaseCount() > 1;
    }

    bool operator()(const TradeCommFrequency_t& rFrequency) const
    {
        const DiplomacyLedger& rLedger = rState.GetDiplomacyLedger();
        return IsThirdParty_(rFrequency.factionId)
            && rLedger.AreKnown(rGiver.GetFactionId(), rFrequency.factionId)
            && !rLedger.AreKnown(rReceiver.GetFactionId(), rFrequency.factionId);
    }

    bool operator()(const TradeWorldMap_t& /*rMap*/) const
    {
        return rGiver.GetExploredMap().IsSized();
    }

    bool operator()(const TradeDeclareVendetta_t& rVendetta) const
    {
        const DiplomacyLedger& rLedger = rState.GetDiplomacyLedger();
        return IsThirdParty_(rVendetta.againstFactionId)
            && rLedger.AreKnown(rGiver.GetFactionId(), rVendetta.againstFactionId)
            && !rLedger.HasVendetta(rGiver.GetFactionId(), rVendetta.againstFactionId);
    }

private:
    bool IsThirdParty_(FactionId_t factionId) const
    {
        return factionId != rGiver.GetFactionId() && factionId != rReceiver.GetFactionId()
            && rState.FindFaction(factionId) != nullptr;
    }
};

// Each base and tech can be handed over once.
bool OffersNothingTwice_(const std::vector<TradeItem_t>& rItems)
{
    std::set<BaseId_t> bases;
    std::set<TechId> techs;
    for (const TradeItem_t& rItem : rItems)
    {
        const auto* pBase = std::get_if<TradeBase_t>(&rItem);
        if (pBase && !bases.insert(pBase->baseId).second)
        {
            return false;
        }
        const auto* pTechnology = std::get_if<TradeTechnology_t>(&rItem);
        if (pTechnology && !techs.insert(pTechnology->techId).second)
        {
            return false;
        }
    }
    return true;
}

int TotalCredits_(const std::vector<TradeItem_t>& rItems)
{
    int total = 0;
    for (const TradeItem_t& rItem : rItems)
    {
        if (const auto* pCredits = std::get_if<TradeCredits_t>(&rItem))
        {
            total += pCredits->amount;
        }
    }
    return total;
}

// However many bases a side hands over, its giver keeps one.
bool KeepsABase_(const Faction& rGiver, const std::vector<TradeItem_t>& rItems)
{
    const auto bases = std::ranges::count_if(
        rItems, [](const TradeItem_t& rItem) { return std::holds_alternative<TradeBase_t>(rItem); });
    return bases == 0 || static_cast<std::size_t>(bases) < rGiver.GetBaseCount();
}

bool IsValidSide_(const GameState& rState, const Faction& rGiver, const Faction& rReceiver,
                  const std::vector<TradeItem_t>& rItems)
{
    for (const TradeItem_t& rItem : rItems)
    {
        if (!std::visit(CanDeliver_{rState, rGiver, rReceiver}, rItem))
        {
            return false;
        }
    }
    return OffersNothingTwice_(rItems) && KeepsABase_(rGiver, rItems)
        && rGiver.GetEconomy().CanAfford(TotalCredits_(rItems));
}

// base_swap refuses to hand over the asked faction's headquarters (NOMYHQ); a base offered or
// given may be the giver's headquarters.
bool IsGiversHeadquarters_(const Faction& rGiver, const TradeItem_t& rItem)
{
    const auto* pBase = std::get_if<TradeBase_t>(&rItem);
    const BaseManager* pHeadquarters = rGiver.GetHeadquarters();
    return pBase && pHeadquarters && pHeadquarters->GetBaseId() == pBase->baseId;
}

bool IsValidDemand_(const GameState& rState, const Faction& rGiver, const Faction& rReceiver,
                    const std::vector<TradeItem_t>& rItems)
{
    const auto isHeadquarters = [&rGiver](const TradeItem_t& rItem)
    { return IsGiversHeadquarters_(rGiver, rItem); };
    return IsValidSide_(rState, rGiver, rReceiver, rItems)
        && std::ranges::none_of(rItems, isHeadquarters);
}

bool IsValidStatusRequest_(const DiplomacyLedger& rLedger, const DiplomaticProposal_t& rProposal)
{
    if (!rProposal.requestedStatus)
    {
        return true;
    }
    return CanProposeStepUp(rLedger, rProposal.proposer, rProposal.recipient)
        && StepUp(rLedger.GetStatus(rProposal.proposer, rProposal.recipient))
               == rProposal.requestedStatus;
}

} // namespace

bool IsValidProposal(const DiplomaticProposal_t& rProposal)
{
    const GameState& rGameState = CurrentGameState();
    const Faction* pProposer = rGameState.FindFaction(rProposal.proposer);
    const Faction* pRecipient = rGameState.FindFaction(rProposal.recipient);
    if (!pProposer || !pRecipient || pProposer == pRecipient)
    {
        return false;
    }
    const DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    if (!rLedger.AreKnown(rProposal.proposer, rProposal.recipient))
    {
        return false;
    }
    if (!rProposal.requestedStatus && rProposal.give.empty() && rProposal.demand.empty())
    {
        return false;
    }
    return IsValidStatusRequest_(rLedger, rProposal)
        && IsValidSide_(rGameState, *pProposer, *pRecipient, rProposal.give)
        && IsValidDemand_(rGameState, *pRecipient, *pProposer, rProposal.demand);
}

bool CanDeliver(FactionId_t giver, FactionId_t receiver, const TradeItem_t& rItem)
{
    const GameState& rGameState = CurrentGameState();
    const Faction& rGiver = rGameState.RequireFaction(giver);
    const Faction& rReceiver = rGameState.RequireFaction(receiver);
    return std::visit(CanDeliver_{rGameState, rGiver, rReceiver}, rItem);
}

bool CanDemand(FactionId_t giver, FactionId_t receiver, const TradeItem_t& rItem)
{
    const Faction& rGiver = CurrentGameState().RequireFaction(giver);
    return CanDeliver(giver, receiver, rItem) && !IsGiversHeadquarters_(rGiver, rItem);
}

} // namespace ac
