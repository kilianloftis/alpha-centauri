#include "ui/commlinks/TalkBindings.h"

#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/faction/Faction.h"
#include "ui/StandingCaptions.h"
#include "ui/text/TextPack.h"

#include <stdexcept>

namespace ac
{

namespace
{

// labels.txt numbers get_pact and get_pact_hood read (terranx-map.md, Talk).
constexpr int k_PactBrotherLabel = 201;
constexpr int k_PactSisterLabel = 202;
constexpr int k_PactOfBrotherhoodLabel = 205;
constexpr int k_PactOfSisterhoodLabel = 206;

} // namespace

TalkBindings::TalkBindings(const GameState& rGameState, const TextPack& rText,
                           const StandingCaptions& rCaptions, FactionId_t playerId,
                           FactionId_t rivalId)
    : m_rGameState(rGameState)
    , m_rText(rText)
    , m_rCaptions(rCaptions)
    , m_playerId(playerId)
    , m_rivalId(rivalId)
{
}

ScriptArgs TalkBindings::Args(const TalkBinding_t& rBinding) const
{
    if (!rBinding.numbers.empty())
    {
        throw std::logic_error("TalkBindings::Args: number slots need a trade row");
    }
    ScriptArgs args;
    for (const auto& [rSlot, rSource] : rBinding.strings)
    {
        args.SetString(rSlot, Value_(rSource));
    }
    return args;
}

ScriptArgs TalkBindings::CandidateArgs(const TalkBinding_t& rBinding,
                                       const TalkCandidate_t& rCandidate) const
{
    ScriptArgs args;
    for (const auto& [rSlot, rSource] : rBinding.strings)
    {
        args.SetString(rSlot, rSource.kind == TalkValueKind_t::CandidateName
                                  ? ScriptValue_t{rCandidate.name}
                                  : Value_(rSource));
    }
    for (const auto& [rSlot, rSource] : rBinding.numbers)
    {
        args.SetNumber(rSlot, rCandidate.amount);
    }
    return args;
}

std::string TalkBindings::Text(const TalkTextRef_t& rRef) const
{
    return FormatScript(Template_(rRef), Args(rRef.binding));
}

std::string TalkBindings::CandidateText(const TalkTextRef_t& rRef,
                                        const TalkCandidate_t& rCandidate) const
{
    return FormatScript(Template_(rRef), CandidateArgs(rRef.binding, rCandidate));
}

const FactionVocabulary_t& TalkBindings::Vocabulary(FactionId_t factionId) const
{
    return m_rText.Vocabulary(m_rGameState.RequireFaction(factionId).GetDefinitionId());
}

ScriptValue_t TalkBindings::Value_(const TalkValueSource_t& rSource) const
{
    switch (rSource.kind)
    {
    case TalkValueKind_t::Field:
        return SideVocabulary_(rSource.side).fields.at(rSource.key);
    case TalkValueKind_t::Phrase:
        return ScriptValue_t{SideVocabulary_(rSource.side).phrases.at(rSource.key)};
    case TalkValueKind_t::NounPhrase:
        return NounPhrase(SideVocabulary_(rSource.side).phrases.at(rSource.key));
    case TalkValueKind_t::PactBrother:
        return PactBrother_(rSource.side);
    case TalkValueKind_t::PactHood:
        return PactHood_();
    case TalkValueKind_t::Caption:
        return Caption();
    case TalkValueKind_t::CandidateName:
    case TalkValueKind_t::CandidateAmount:
        break;
    }
    throw std::logic_error("TalkBindings: a candidate value outside a trade row");
}

// get_pact: Pact Brother or Pact Sister by the leader's sex, singular.
ScriptValue_t TalkBindings::PactBrother_(TalkSide_t side) const
{
    const bool bFemale = IsLeaderFemale_(side);
    const int label = bFemale ? k_PactSisterLabel : k_PactBrotherLabel;
    return ScriptValue_t{m_rText.Label(label),
                         bFemale ? GrammaticalGender_t::Feminine : GrammaticalGender_t::Masculine,
                         false};
}

// get_pact_hood: Sisterhood only when both leaders are female; masculine singular either way.
ScriptValue_t TalkBindings::PactHood_() const
{
    const bool bSisters = IsLeaderFemale_(TalkSide_t::Player) && IsLeaderFemale_(TalkSide_t::Rival);
    const int label = bSisters ? k_PactOfSisterhoodLabel : k_PactOfBrotherhoodLabel;
    return ScriptValue_t{m_rText.Label(label)};
}

ScriptValue_t TalkBindings::Caption() const
{
    // TODO: SMAC reads label 488 Submissive in place of the mood when the band is 0 and
    // 0x93FA98 is set without a Pact, or after a surrender (terranx-map.md, diplomacy_caption).
    // There is no surrender state.
    const DiplomacyLedger& rLedger = m_rGameState.GetDiplomacyLedger();
    const int friction = rLedger.GetFriction(m_rivalId, m_playerId);
    const DiplomaticStatus_t status = rLedger.GetStatus(m_playerId, m_rivalId);
    ScriptValue_t caption = SideVocabulary_(TalkSide_t::Rival).fields.at("noun_faction");
    caption.text += " - " + m_rCaptions.CaptionMood(friction, status);
    return caption;
}

const FactionVocabulary_t& TalkBindings::SideVocabulary_(TalkSide_t side) const
{
    return Vocabulary(side == TalkSide_t::Player ? m_playerId : m_rivalId);
}

bool TalkBindings::IsLeaderFemale_(TalkSide_t side) const
{
    return SideVocabulary_(side).fields.at("name_leader").gender == GrammaticalGender_t::Feminine;
}

const std::string& TalkBindings::Template_(const TalkTextRef_t& rRef) const
{
    const ScriptEntry_t& rEntry = m_rText.Entry(rRef.id);
    return rRef.item ? rEntry.items.at(static_cast<std::size_t>(*rRef.item)) : rEntry.body;
}

} // namespace ac
