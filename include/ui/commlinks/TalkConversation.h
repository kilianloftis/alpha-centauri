#pragma once

#include "ui/ListSelectorPopup.h"
#include "ui/commlinks/TalkBindings.h"
#include "ui/commlinks/TalkScript.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace ac
{

class Faction;
class GameState;
struct DiplomaticProposal_t;

// One item on a side of the trade being built, with the label it was picked under.
struct TalkDraftItem_t
{
    TradeItem_t item;
    std::string label;
};

struct TalkDraftHeadings_t
{
    std::string offer;
    std::string ask;
};

// One talk with a rival, run through the TalkScript: the scene, the rival's last line, and the
// trade being built. Choosing a row runs its action and records what follows; the caller asks
// for the next scene's rows afterwards.
class TalkConversation
{
public:
    TalkConversation(GameState& rGameState, const TalkScript& rScript, const TextPack& rText,
                     const StandingCaptions& rCaptions, FactionId_t rivalId, uint32_t seed);

    bool IsOver() const { return m_scene == TalkScript::k_EndScene; }
    const std::string& Speech() const { return m_speech; }
    std::string Prompt() const;
    std::vector<PopupChoice_t> Choices();
    // Present when the scene lists the draft.
    std::optional<TalkDraftHeadings_t> DraftHeadings() const;
    const std::vector<TalkDraftItem_t>& Offer() const { return m_offer; }
    const std::vector<TalkDraftItem_t>& Ask() const { return m_ask; }

    const TalkBindings& Bindings() const { return m_bindings; }
    FactionId_t PlayerId() const { return m_playerId; }
    FactionId_t RivalId() const { return m_rivalId; }

private:
    struct Pick_t
    {
        TradeKind_t kind = TradeKind_t::Credits;
        TradeSide_t side = TradeSide_t::Offer;
    };

    void AddRows_(std::vector<PopupChoice_t>& rRows, const TalkFixedChoice_t& rChoice);
    void AddRows_(std::vector<PopupChoice_t>& rRows, const TalkKindRows_t& rKindRows);
    void AddRows_(std::vector<PopupChoice_t>& rRows, const TalkCandidateRows_t& rCandidateRows);

    void Choose_(const TalkFixedChoice_t& rChoice);
    void Follow_(const TalkOutcome_t& rOutcome);
    std::string Say_(const std::vector<TalkTextRef_t>& rLines);

    bool Offers_(DiplomaticActionKind_t kind) const;
    bool HasDraft_() const;
    bool IsAvailable_(TalkAction_t action) const;
    TalkResult_t Run_(TalkAction_t action);
    TalkResult_t Propose_(const DiplomaticProposal_t& rProposal);
    DiplomaticProposal_t StepUpProposal_() const;
    DiplomaticProposal_t TradeProposal_() const;
    void ClearDraft_();

    // Every item of the kind the side's giver could hand over, as MayDraft_ allows.
    std::vector<TalkCandidate_t> Candidates_(TradeKind_t kind, TradeSide_t side) const;
    std::vector<TalkCandidate_t> ItemsOf_(TradeKind_t kind, FactionId_t giver) const;
    std::string NameOf_(const TradeItem_t& rItem, const Faction& rGiver) const;
    FactionId_t GiverOf_(TradeSide_t side) const;
    // CanDeliver for what the player offers, CanDemand for what it asks.
    bool MayDraft_(TradeSide_t side, const TradeItem_t& rItem) const;
    std::vector<TalkDraftItem_t>& SideOf_(TradeSide_t side);

    GameState& m_rGameState;
    const TalkScript& m_rScript;
    FactionId_t m_playerId;
    FactionId_t m_rivalId;
    TalkBindings m_bindings;
    std::mt19937 m_rng;
    std::string m_scene;
    std::string m_speech;
    std::vector<TalkDraftItem_t> m_offer;
    std::vector<TalkDraftItem_t> m_ask;
    Pick_t m_pick;
};

} // namespace ac
