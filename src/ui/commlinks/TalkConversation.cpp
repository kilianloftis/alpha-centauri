#include "ui/commlinks/TalkConversation.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/diplomacy/DiplomacyActions.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticActionExecutor.h"
#include "game/diplomacy/DiplomaticProposalRules.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechRegistry.h"
#include "ui/text/TextPack.h"

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace ac
{

TalkConversation::TalkConversation(GameState& rGameState, const TalkScript& rScript,
                                   const TextPack& rText, const StandingCaptions& rCaptions,
                                   FactionId_t rivalId, uint32_t seed)
    : m_rGameState(rGameState)
    , m_rScript(rScript)
    , m_playerId(rGameState.GetPlayerFaction().GetFactionId())
    , m_rivalId(rivalId)
    , m_bindings(rGameState, rText, rCaptions, m_playerId, rivalId)
    , m_rng(seed)
{
    Follow_(rScript.Start());
}

std::string TalkConversation::Prompt() const
{
    return m_bindings.Text(m_rScript.Scene(m_scene).prompt);
}

std::vector<PopupChoice_t> TalkConversation::Choices()
{
    std::vector<PopupChoice_t> rows;
    for (const TalkChoice_t& rChoice : m_rScript.Scene(m_scene).choices)
    {
        std::visit([this, &rows](const auto& rRow) { AddRows_(rows, rRow); }, rChoice);
    }
    return rows;
}

std::optional<TalkDraftHeadings_t> TalkConversation::DraftHeadings() const
{
    const std::optional<TalkDraftLabels_t>& rDraft = m_rScript.Scene(m_scene).draft;
    if (!rDraft)
    {
        return std::nullopt;
    }
    return TalkDraftHeadings_t{m_bindings.Text(rDraft->offer), m_bindings.Text(rDraft->ask)};
}

void TalkConversation::AddRows_(std::vector<PopupChoice_t>& rRows,
                                const TalkFixedChoice_t& rChoice)
{
    if (rChoice.when && !Holds_(*rChoice.when))
    {
        return;
    }
    if (rChoice.action && !IsAvailable_(*rChoice.action))
    {
        return;
    }
    const TalkFixedChoice_t* pChoice = &rChoice;
    std::string label = WithoutHighlights(m_bindings.Text(rChoice.label));
    rRows.push_back({std::move(label), [this, pChoice] { Choose_(*pChoice); }});
}

void TalkConversation::AddRows_(std::vector<PopupChoice_t>& rRows,
                                const TalkKindRows_t& rKindRows)
{
    for (const TradeKind_t kind : TradeKinds())
    {
        if (Candidates_(kind, rKindRows.side).empty())
        {
            continue;
        }
        const Pick_t pick{kind, rKindRows.side};
        const std::string* pThen = &rKindRows.then;
        std::string label = WithoutHighlights(m_bindings.Text(rKindRows.labels.at(kind)));
        rRows.push_back({std::move(label), [this, pick, pThen] {
                             m_pick = pick;
                             m_scene = *pThen;
                         }});
    }
}

void TalkConversation::AddRows_(std::vector<PopupChoice_t>& rRows,
                                const TalkCandidateRows_t& rCandidateRows)
{
    const TalkTextRef_t& rLabel = rCandidateRows.labels.at(m_pick.kind);
    const std::string* pThen = &rCandidateRows.then;
    for (const TalkCandidate_t& rCandidate : Candidates_(m_pick.kind, m_pick.side))
    {
        TalkDraftItem_t drafted{rCandidate.item, m_bindings.CandidateText(rLabel, rCandidate)};
        std::string label = WithoutHighlights(drafted.label);
        const TradeSide_t side = m_pick.side;
        rRows.push_back({std::move(label), [this, drafted, side, pThen] {
                             SideOf_(side).push_back(drafted);
                             m_scene = *pThen;
                         }});
    }
}

void TalkConversation::Choose_(const TalkFixedChoice_t& rChoice)
{
    const TalkResult_t result = rChoice.action ? Run_(*rChoice.action) : TalkResult_t::Done;
    Follow_(rChoice.outcomes.at(result));
}

void TalkConversation::Follow_(const TalkOutcome_t& rOutcome)
{
    if (!rOutcome.say.empty())
    {
        m_speech = Say_(rOutcome.say);
    }
    m_scene = rOutcome.then;
}

// SMAC appends random(n) to the label; the script lists the variants instead.
std::string TalkConversation::Say_(const std::vector<TalkTextRef_t>& rLines)
{
    std::uniform_int_distribution<std::size_t> pick(0, rLines.size() - 1);
    return m_bindings.Text(rLines[pick(m_rng)]);
}

bool TalkConversation::Holds_(TalkCondition_t condition) const
{
    const std::vector<DiplomaticActionKind_t> actions =
        GetTalkActions(m_rGameState.GetDiplomacyLedger(), m_playerId, m_rivalId);
    const auto offers = [&actions](DiplomaticActionKind_t kind)
    { return std::ranges::find(actions, kind) != actions.end(); };
    switch (condition)
    {
    case TalkCondition_t::MayProposeTruce:
        return offers(DiplomaticActionKind_t::ProposeTruce);
    case TalkCondition_t::MayProposeTreaty:
        return offers(DiplomaticActionKind_t::ProposeTreaty);
    case TalkCondition_t::MayProposePact:
        return offers(DiplomaticActionKind_t::ProposePact);
    case TalkCondition_t::MayRenouncePact:
        return offers(DiplomaticActionKind_t::CancelTreaty);
    case TalkCondition_t::MayTrade:
        return offers(DiplomaticActionKind_t::Trade);
    case TalkCondition_t::HasDraft:
        return !m_offer.empty() || !m_ask.empty();
    }
    throw std::logic_error("TalkConversation: unhandled TalkCondition_t");
}

bool TalkConversation::IsAvailable_(TalkAction_t action) const
{
    switch (action)
    {
    case TalkAction_t::ProposeTruce:
        return Holds_(TalkCondition_t::MayProposeTruce);
    case TalkAction_t::ProposeTreaty:
        return Holds_(TalkCondition_t::MayProposeTreaty);
    case TalkAction_t::ProposePact:
        return Holds_(TalkCondition_t::MayProposePact);
    case TalkAction_t::ProposeTrade:
        return Holds_(TalkCondition_t::MayTrade) && Holds_(TalkCondition_t::HasDraft);
    case TalkAction_t::RenouncePact:
        return Holds_(TalkCondition_t::MayRenouncePact);
    case TalkAction_t::ClearTrade:
        return Holds_(TalkCondition_t::HasDraft);
    }
    throw std::logic_error("TalkConversation: unhandled TalkAction_t");
}

TalkResult_t TalkConversation::Run_(TalkAction_t action)
{
    switch (action)
    {
    case TalkAction_t::ProposeTruce:
    case TalkAction_t::ProposeTreaty:
    case TalkAction_t::ProposePact:
        return Propose_(StepUpProposal_());
    case TalkAction_t::ProposeTrade:
    {
        const TalkResult_t result = Propose_(TradeProposal_());
        if (result == TalkResult_t::Accepted)
        {
            ClearDraft_();
        }
        return result;
    }
    case TalkAction_t::RenouncePact:
        CancelTreaty(m_rGameState, m_playerId, m_rivalId);
        return TalkResult_t::Done;
    case TalkAction_t::ClearTrade:
        ClearDraft_();
        return TalkResult_t::Done;
    }
    throw std::logic_error("TalkConversation: unhandled TalkAction_t");
}

TalkResult_t TalkConversation::Propose_(const DiplomaticProposal_t& rProposal)
{
    switch (m_rGameState.GetDiplomaticActionExecutor().Propose(m_rGameState, rProposal))
    {
    case DiplomaticProposeResult_t::Accepted:
        return TalkResult_t::Accepted;
    case DiplomaticProposeResult_t::Rejected:
        return TalkResult_t::Rejected;
    case DiplomaticProposeResult_t::Invalid:
        return TalkResult_t::Invalid;
    case DiplomaticProposeResult_t::PendingPlayer:
    case DiplomaticProposeResult_t::Busy:
        break;
    }
    throw std::logic_error("TalkConversation: a talk's recipient is an AI, which answers at once");
}

DiplomaticProposal_t TalkConversation::StepUpProposal_() const
{
    const DiplomaticStatus_t status =
        m_rGameState.GetDiplomacyLedger().GetStatus(m_playerId, m_rivalId);
    return DiplomaticProposal_t{m_playerId, m_rivalId, StepUp(status), {}, {}};
}

DiplomaticProposal_t TalkConversation::TradeProposal_() const
{
    DiplomaticProposal_t proposal{m_playerId, m_rivalId, std::nullopt, {}, {}};
    for (const TalkDraftItem_t& rItem : m_offer)
    {
        proposal.give.push_back(rItem.item);
    }
    for (const TalkDraftItem_t& rItem : m_ask)
    {
        proposal.demand.push_back(rItem.item);
    }
    return proposal;
}

void TalkConversation::ClearDraft_()
{
    m_offer.clear();
    m_ask.clear();
}

std::vector<TalkCandidate_t> TalkConversation::Candidates_(TradeKind_t kind,
                                                           TradeSide_t side) const
{
    const FactionId_t giver = GiverOf_(side);
    const FactionId_t receiver = ReceiverOf_(side);
    std::vector<TalkCandidate_t> candidates = ItemsOf_(kind, giver);
    std::erase_if(candidates, [giver, receiver](const TalkCandidate_t& rCandidate)
                  { return !CanDeliver(giver, receiver, rCandidate.item); });
    const Faction& rGiver = m_rGameState.RequireFaction(giver);
    for (TalkCandidate_t& rCandidate : candidates)
    {
        rCandidate.name = NameOf_(rCandidate.item, rGiver);
    }
    return candidates;
}

std::vector<TalkCandidate_t> TalkConversation::ItemsOf_(TradeKind_t kind, FactionId_t giver) const
{
    const Faction& rGiver = m_rGameState.RequireFaction(giver);
    std::vector<TalkCandidate_t> items;
    switch (kind)
    {
    case TradeKind_t::Credits:
        for (const int amount : m_rScript.CreditOffers())
        {
            items.push_back({TradeCredits_t{amount}, "", amount});
        }
        break;
    case TradeKind_t::Technology:
        for (const TechId& rTech : rGiver.GetResearch().GetDiscoveredTechs())
        {
            items.push_back({TradeTechnology_t{rTech}});
        }
        break;
    case TradeKind_t::Base:
        for (const BaseManager& rBase : rGiver.Bases())
        {
            items.push_back({TradeBase_t{static_cast<BaseId_t>(rBase.GetBaseId())}});
        }
        break;
    case TradeKind_t::CommFrequency:
        for (const Faction& rFaction : m_rGameState.Factions())
        {
            items.push_back({TradeCommFrequency_t{rFaction.GetFactionId()}});
        }
        break;
    case TradeKind_t::WorldMap:
        items.push_back({TradeWorldMap_t{}});
        break;
    case TradeKind_t::DeclareVendetta:
        for (const Faction& rFaction : m_rGameState.Factions())
        {
            items.push_back({TradeDeclareVendetta_t{rFaction.GetFactionId()}});
        }
        break;
    }
    return items;
}

// What a candidate label's name slot reads: the tech, base or faction it names.
std::string TalkConversation::NameOf_(const TradeItem_t& rItem, const Faction& rGiver) const
{
    if (const auto* pTech = std::get_if<TradeTechnology_t>(&rItem))
    {
        return m_rGameState.GetGameData().techRegistry->Get(pTech->techId).name;
    }
    if (const auto* pBase = std::get_if<TradeBase_t>(&rItem))
    {
        return rGiver.FindBase(pBase->baseId)->GetName();
    }
    if (const auto* pFrequency = std::get_if<TradeCommFrequency_t>(&rItem))
    {
        return m_bindings.Vocabulary(pFrequency->factionId).fields.at("noun_faction").text;
    }
    if (const auto* pVendetta = std::get_if<TradeDeclareVendetta_t>(&rItem))
    {
        return m_bindings.Vocabulary(pVendetta->againstFactionId).fields.at("noun_faction").text;
    }
    return "";
}

FactionId_t TalkConversation::GiverOf_(TradeSide_t side) const
{
    return side == TradeSide_t::Offer ? m_playerId : m_rivalId;
}

FactionId_t TalkConversation::ReceiverOf_(TradeSide_t side) const
{
    return side == TradeSide_t::Offer ? m_rivalId : m_playerId;
}

std::vector<TalkDraftItem_t>& TalkConversation::SideOf_(TradeSide_t side)
{
    return side == TradeSide_t::Offer ? m_offer : m_ask;
}

} // namespace ac
