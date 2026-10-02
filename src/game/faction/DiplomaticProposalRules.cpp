#include "game/faction/DiplomaticProposalRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticTransitionRules.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/ResearchManager.h"

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

    bool operator()(const TradeBase_t& rBase) const
    {
        return rGiver.FindBase(rBase.baseId) != nullptr;
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
    return OffersNothingTwice_(rItems) && rGiver.GetEconomy().CanAfford(TotalCredits_(rItems));
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

bool IsValidProposal(const GameState& rGameState, const DiplomaticProposal_t& rProposal)
{
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
        && IsValidSide_(rGameState, *pRecipient, *pProposer, rProposal.demand);
}

} // namespace ac
