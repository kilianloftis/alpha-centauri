#include "game/faction/DiplomaticProposalEffects.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/ResearchManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <variant>
#include <vector>

namespace ac
{

namespace
{

// Hands one item from rGiver to rReceiver.
struct Deliver_
{
    GameState& rState;
    Faction& rGiver;
    Faction& rReceiver;

    void operator()(const TradeCredits_t& rCredits) const
    {
        rGiver.GetEconomy().SpendEnergy(rCredits.amount);
        rReceiver.GetEconomy().AddEnergy(rCredits.amount);
    }

    void operator()(const TradeTechnology_t& rTechnology) const
    {
        rReceiver.GetResearch().AddDiscoveredTech(rTechnology.techId);
    }

    void operator()(const TradeBase_t& rBase) const
    {
        rGiver.TransferBaseTo(rBase.baseId, rReceiver);
    }

    void operator()(const TradeCommFrequency_t& rFrequency) const
    {
        rState.GetDiplomacyLedger().SetKnown(rReceiver.GetFactionId(), rFrequency.factionId);
    }

    void operator()(const TradeWorldMap_t& /*rMap*/) const
    {
        const FactionExploredMap& rGiverExplored = rGiver.GetExploredMap();
        FactionExploredMap& rReceiverExplored = rReceiver.GetExploredMap();
        std::vector<const Tile*> traded;
        for (const auto& pOwnedTile : rReceiver.GetWorldMap().GetTiles())
        {
            if (pOwnedTile && rGiverExplored.IsExplored(*pOwnedTile)
                && !rReceiverExplored.IsExplored(*pOwnedTile))
            {
                traded.push_back(pOwnedTile.get());
            }
        }
        rReceiverExplored.MergeFrom(rGiverExplored);
        // TODO: confirm in terranx.exe what a faction sees on tiles explored by map trade or shroud removal
        for (const Tile* pTile : traded)
        {
            rReceiver.GetTileMemory().Record(*pTile);
        }
    }

    void operator()(const TradeDeclareVendetta_t& rVendetta) const
    {
        DeclareVendetta(rState, rGiver.GetFactionId(), rVendetta.againstFactionId);
    }
};

void DeliverAll_(GameState& rState, Faction& rGiver, Faction& rReceiver,
                 const std::vector<TradeItem_t>& rItems)
{
    for (const TradeItem_t& rItem : rItems)
    {
        std::visit(Deliver_{rState, rGiver, rReceiver}, rItem);
    }
}

} // namespace

void ApplyProposal(GameState& rGameState, const DiplomaticProposal_t& rProposal)
{
    Faction& rProposer = rGameState.RequireFaction(rProposal.proposer);
    Faction& rRecipient = rGameState.RequireFaction(rProposal.recipient);
    if (rProposal.requestedStatus)
    {
        ApplyStatusChange(rGameState, rProposal.proposer, rProposal.recipient,
                          *rProposal.requestedStatus);
    }
    DeliverAll_(rGameState, rProposer, rRecipient, rProposal.give);
    DeliverAll_(rGameState, rRecipient, rProposer, rProposal.demand);
}

} // namespace ac
