#pragma once

#include "game/diplomacy/DiplomacyActions.h"
#include "game/diplomacy/TradeItem.h"

#include "lib/config/ConfigReader.h"

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

class FactionRegistry;
class TextPack;

enum class TalkSide_t
{
    Player,
    Rival
};

// Where a slot's value comes from. Each mirrors the SMAC getter named beside it
// (terranx-map.md, Talk).
enum class TalkValueKind_t
{
    Field,           // an MFaction header field, by name
    Phrase,          // get_phrase
    NounPhrase,      // get_noun_phrase
    PactBrother,     // get_pact
    PactHood,        // get_pact_hood(player, rival)
    Caption,         // diplomacy_caption(player, rival)
    CandidateName,   // a trade row's item
    CandidateAmount  // a trade row's credits, a number slot
};

struct TalkValueSource_t
{
    TalkValueKind_t kind = TalkValueKind_t::Field;
    TalkSide_t side = TalkSide_t::Player;
    std::string key; // the vocabulary field or phrase
};

// The slots one text reference fills, as the parse_says block before an X_pops does.
struct TalkBinding_t
{
    std::map<int, TalkValueSource_t> strings;
    std::map<int, TalkValueSource_t> numbers;
};

// A script entry's body, or one of its items.
struct TalkTextRef_t
{
    std::string id;
    std::optional<int> item;
    TalkBinding_t binding;
};

// What follows a choice: the rival's line, picked from the list at random, then a scene.
struct TalkOutcome_t
{
    std::vector<TalkTextRef_t> say;
    std::string then;
};

enum class TalkAction_t
{
    ProposeTruce,
    ProposeTreaty,
    ProposePact,
    ProposeTrade,
    RenouncePact,
    ClearTrade
};

enum class TalkResult_t
{
    Accepted,
    Rejected,
    Invalid,
    Done
};

enum class TradeSide_t
{
    Offer,
    Ask
};

// A row written in the script. Its outcomes cover every result its action can return, or Done
// when it runs none.
struct TalkFixedChoice_t
{
    TalkTextRef_t label;
    // Shown only while GetTalkActions offers this.
    std::optional<DiplomaticActionKind_t> when;
    std::optional<TalkAction_t> action;
    std::map<TalkResult_t, TalkOutcome_t> outcomes;
};

// One row per trade kind with something to give on that side.
struct TalkKindRows_t
{
    TradeSide_t side = TradeSide_t::Offer;
    std::map<TradeKind_t, TalkTextRef_t> labels;
    std::string then;
};

// One row per deliverable item of the kind and side being picked.
struct TalkCandidateRows_t
{
    std::map<TradeKind_t, TalkTextRef_t> labels;
    std::string then;
};

using TalkChoice_t = std::variant<TalkFixedChoice_t, TalkKindRows_t, TalkCandidateRows_t>;

struct TalkDraftLabels_t
{
    TalkTextRef_t offer;
    TalkTextRef_t ask;
};

struct TalkScene_t
{
    TalkTextRef_t prompt;
    std::vector<TalkChoice_t> choices;
    // The trade scenes list the draft under the rival's line.
    std::optional<TalkDraftLabels_t> draft;
};

// The labels the talk portrait draws.
struct TalkPortrait_t
{
    int mood = 0;
    int reputation = 0;
    int integrity = 0;
};

// The talk flow from config/ui/talk.json: scenes, choices, the action each runs, and the text
// and slot bindings of every line. Load checks it against the text pack and every faction's
// vocabulary, so a talk never meets a missing id or an unbound slot.
class TalkScript
{
public:
    static constexpr const char* k_EndScene = "end";

    static TalkScript Load(const std::string& rPath, const TextPack& rText,
                           const FactionRegistry& rFactions);

    const TalkPortrait_t& Portrait() const { return m_portrait; }
    // The credit amounts a trade row offers.
    const std::vector<int>& CreditOffers() const { return m_creditOffers; }
    const TalkOutcome_t& Start() const { return m_start; }
    // Throws for a name no scene has; k_EndScene is not a scene.
    const TalkScene_t& Scene(const std::string& rName) const;

    static std::vector<TalkResult_t> ResultsOf(TalkAction_t action);

private:
    TalkScript(TalkPortrait_t portrait, std::vector<int> creditOffers, TalkOutcome_t start,
               std::map<std::string, TalkScene_t> scenes);

    static TalkScript Parse_(const ConfigReader& rReader, const TextPack& rText,
                             const FactionRegistry& rFactions);

    TalkPortrait_t m_portrait;
    std::vector<int> m_creditOffers;
    TalkOutcome_t m_start;
    std::map<std::string, TalkScene_t> m_scenes;
};

} // namespace ac
