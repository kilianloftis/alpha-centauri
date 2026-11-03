#pragma once

#include "game/base/BaseTypes.h"
#include "ui/commlinks/TalkScript.h"
#include "ui/text/ScriptMarkup.h"

namespace ac
{

class GameState;
class StandingCaptions;
class TextPack;
struct FactionVocabulary_t;

// A trade row's item as its label names it.
struct TalkCandidate_t
{
    TradeItem_t item;
    std::string name;
    int amount = 0;
};

// Fills a text reference's slots for one talk, the way the SMAC getters beside each
// TalkValueKind_t do.
class TalkBindings
{
public:
    TalkBindings(const GameState& rGameState, const TextPack& rText,
                 const StandingCaptions& rCaptions, FactionId_t playerId, FactionId_t rivalId);

    ScriptArgs Args(const TalkBinding_t& rBinding) const;
    ScriptArgs CandidateArgs(const TalkBinding_t& rBinding, const TalkCandidate_t& rCandidate) const;

    // The formatted text of rRef, braces kept for HighlightRuns.
    std::string Text(const TalkTextRef_t& rRef) const;
    std::string CandidateText(const TalkTextRef_t& rRef, const TalkCandidate_t& rCandidate) const;

    const FactionVocabulary_t& Vocabulary(FactionId_t factionId) const;
    // diplomacy_caption(player, rival): the rival's noun, " - ", and its mood toward the player.
    ScriptValue_t Caption() const;

private:
    ScriptValue_t Value_(const TalkValueSource_t& rSource) const;
    ScriptValue_t PactBrother_(TalkSide_t side) const;
    ScriptValue_t PactHood_() const;
    const FactionVocabulary_t& SideVocabulary_(TalkSide_t side) const;
    bool IsLeaderFemale_(TalkSide_t side) const;
    const std::string& Template_(const TalkTextRef_t& rRef) const;

    const GameState& m_rGameState;
    const TextPack& m_rText;
    const StandingCaptions& m_rCaptions;
    FactionId_t m_playerId;
    FactionId_t m_rivalId;
};

} // namespace ac
