#include "ui/commlinks/TalkScript.h"

#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"
#include "ui/text/ScriptMarkup.h"
#include "ui/text/TextPack.h"

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{

// An enumerator named by an object key (a map entry's name) rather than a value.
template <typename Enum_t>
Enum_t EnumFromKey_(const ConfigReader& rReader, const std::string& rKey, const char* pWhat)
{
    try
    {
        return EnumFromName<Enum_t>(rKey, pWhat);
    }
    catch (const std::runtime_error&)
    {
        rReader.FailKey(rKey, std::string("is not a known ") + pWhat);
    }
}

std::vector<std::string> Split_(const std::string& rText, char separator)
{
    std::vector<std::string> parts;
    std::size_t from = 0;
    while (true)
    {
        const std::size_t at = rText.find(separator, from);
        parts.push_back(rText.substr(from, at - from));
        if (at == std::string::npos)
        {
            return parts;
        }
        from = at + 1;
    }
}

int ParseIndex_(const std::string& rText, const std::string& rContext)
{
    const bool bDigits =
        !rText.empty() && std::ranges::all_of(rText, [](char c) { return c >= '0' && c <= '9'; });
    if (!bDigits)
    {
        throw std::runtime_error(rContext + ": '" + rText + "' is not an index");
    }
    return std::stoi(rText);
}

TalkSide_t ParseSide_(const std::string& rText, const std::string& rContext)
{
    if (rText == "player")
    {
        return TalkSide_t::Player;
    }
    if (rText == "rival")
    {
        return TalkSide_t::Rival;
    }
    throw std::runtime_error(rContext + ": unknown side '" + rText + "'");
}

// "pact_hood", "caption", "candidate.name", "candidate.amount", or "<side>." followed by
// "pact_brother", "phrase.<name>", "noun_phrase.<name>" or a vocabulary field.
TalkValueSource_t ParseSource_(const std::string& rText, const std::string& rContext)
{
    const std::string context = rContext + " '" + rText + "'";
    if (rText == "pact_hood")
    {
        return TalkValueSource_t{TalkValueKind_t::PactHood};
    }
    if (rText == "caption")
    {
        return TalkValueSource_t{TalkValueKind_t::Caption};
    }
    if (rText == "candidate.name")
    {
        return TalkValueSource_t{TalkValueKind_t::CandidateName};
    }
    if (rText == "candidate.amount")
    {
        return TalkValueSource_t{TalkValueKind_t::CandidateAmount};
    }
    const std::vector<std::string> parts = Split_(rText, '.');
    const TalkSide_t side = ParseSide_(parts.front(), context);
    if (parts.size() == 2 && parts[1] == "pact_brother")
    {
        return TalkValueSource_t{TalkValueKind_t::PactBrother, side};
    }
    if (parts.size() == 3 && (parts[1] == "phrase" || parts[1] == "noun_phrase"))
    {
        const TalkValueKind_t kind =
            parts[1] == "phrase" ? TalkValueKind_t::Phrase : TalkValueKind_t::NounPhrase;
        return TalkValueSource_t{kind, side, parts[2]};
    }
    if (parts.size() == 2)
    {
        return TalkValueSource_t{TalkValueKind_t::Field, side, parts[1]};
    }
    throw std::runtime_error(context + ": not a value source");
}

bool IsNumberSource_(const TalkValueSource_t& rSource)
{
    return rSource.kind == TalkValueKind_t::CandidateAmount;
}

bool IsCandidateSource_(const TalkValueSource_t& rSource)
{
    return rSource.kind == TalkValueKind_t::CandidateName
        || rSource.kind == TalkValueKind_t::CandidateAmount;
}

// Keys are string slots "0"–"9" and number slots "#0"–"#9", as `$<#n:…>` names them.
void AddSlots_(TalkBinding_t& rBinding, const ConfigReader& rSlots)
{
    for (const auto& [rKey, rUnused] : rSlots.Json().items())
    {
        const std::string sourceName = rSlots.String(rKey);
        const bool bNumber = rKey.starts_with('#');
        const int slot = ParseIndex_(bNumber ? rKey.substr(1) : rKey, rSlots.Context());
        if (slot > 9)
        {
            rSlots.FailKey(rKey, "is past slot 9");
        }
        const TalkValueSource_t source = ParseSource_(sourceName, rSlots.Context());
        if (IsNumberSource_(source) != bNumber)
        {
            rSlots.FailKey(rKey, "and '" + sourceName + "' disagree on number");
        }
        (bNumber ? rBinding.numbers : rBinding.strings)[slot] = source;
    }
}

class TalkScriptParser_
{
public:
    explicit TalkScriptParser_(const ConfigReader& rReader)
        : m_reader(rReader)
    {
        rReader.RejectUnknownKeys({"portrait", "credit_offers", "bindings", "start", "scenes"});
        const ConfigReader bindings = rReader.Object("bindings");
        for (const auto& [rName, rUnused] : bindings.Json().items())
        {
            AddSlots_(m_bindings[rName], bindings.Object(rName));
        }
    }

    TalkPortrait_t Portrait() const
    {
        const ConfigReader portrait = m_reader.Object("portrait");
        portrait.RejectUnknownKeys({"mood", "reputation", "integrity"});
        return TalkPortrait_t{portrait.IntAtLeast("mood", 0), portrait.IntAtLeast("reputation", 0),
                              portrait.IntAtLeast("integrity", 0)};
    }

    std::vector<int> CreditOffers() const
    {
        std::vector<int> offers;
        for (const nlohmann::json& rOffer : m_reader.NonEmptyArray("credit_offers"))
        {
            if (!rOffer.is_number_integer() || rOffer.get<int>() <= 0)
            {
                m_reader.FailKey("credit_offers", "must hold only positive integers");
            }
            offers.push_back(rOffer.get<int>());
        }
        return offers;
    }

    TalkOutcome_t Start() const
    {
        const ConfigReader start = m_reader.Object("start");
        start.RejectUnknownKeys({"say", "then"});
        return Outcome_(start);
    }

    std::map<std::string, TalkScene_t> Scenes() const
    {
        const ConfigReader scenesReader = m_reader.Object("scenes");
        std::map<std::string, TalkScene_t> scenes;
        for (const auto& [rName, rUnused] : scenesReader.Json().items())
        {
            scenes.emplace(rName, Scene_(scenesReader.Object(rName)));
        }
        return scenes;
    }

private:
    TalkScene_t Scene_(const ConfigReader& rScene) const
    {
        rScene.RejectUnknownKeys({"prompt", "choices", "draft"});
        TalkScene_t scene{TextRef_(rScene.Object("prompt")), {}, std::nullopt};
        for (const ConfigReader& rChoice : rScene.Objects("choices"))
        {
            scene.choices.push_back(Choice_(rChoice));
        }
        if (rScene.Has("draft"))
        {
            scene.draft = Draft_(rScene.Object("draft"));
        }
        return scene;
    }

    TalkDraftLabels_t Draft_(const ConfigReader& rDraft) const
    {
        rDraft.RejectUnknownKeys({"offer", "ask"});
        return TalkDraftLabels_t{TextRef_(rDraft.Object("offer")), TextRef_(rDraft.Object("ask"))};
    }

    TalkChoice_t Choice_(const ConfigReader& rChoice) const
    {
        if (!rChoice.Has("from"))
        {
            return FixedChoice_(rChoice);
        }
        const std::string from = rChoice.String("from");
        if (from == "trade_kinds")
        {
            rChoice.RejectUnknownKeys({"from", "side", "labels", "then"});
            const TradeSide_t side = rChoice.Enum<TradeSide_t>("side", "trade side");
            return TalkKindRows_t{side, KindLabels_(rChoice), rChoice.String("then")};
        }
        if (from == "trade_candidates")
        {
            rChoice.RejectUnknownKeys({"from", "labels", "then"});
            return TalkCandidateRows_t{KindLabels_(rChoice), rChoice.String("then")};
        }
        rChoice.FailKey("from", "is not a known generator: '" + from + "'");
    }

    std::map<TradeKind_t, TalkTextRef_t> KindLabels_(const ConfigReader& rChoice) const
    {
        const ConfigReader labelsReader = rChoice.Object("labels");
        std::map<TradeKind_t, TalkTextRef_t> labels;
        for (const auto& [rKind, rUnused] : labelsReader.Json().items())
        {
            const TradeKind_t kind = EnumFromKey_<TradeKind_t>(labelsReader, rKind, "trade kind");
            labels.emplace(kind, TextRef_(labelsReader.Object(rKind)));
        }
        return labels;
    }

    TalkFixedChoice_t FixedChoice_(const ConfigReader& rChoice) const
    {
        rChoice.RejectUnknownKeys({"label", "when", "do", "say", "then", "on"});
        TalkFixedChoice_t choice{TextRef_(rChoice.Object("label")), std::nullopt, std::nullopt,
                                 {}};
        if (rChoice.Has("when"))
        {
            choice.when = rChoice.Enum<DiplomaticActionKind_t>("when", "diplomatic action");
        }
        if (rChoice.Has("do"))
        {
            choice.action = rChoice.Enum<TalkAction_t>("do", "talk action");
        }
        choice.outcomes = Outcomes_(rChoice, choice.action);
        return choice;
    }

    // `then` sends every result one way; `on` gives each result its own.
    std::map<TalkResult_t, TalkOutcome_t> Outcomes_(const ConfigReader& rChoice,
                                                    std::optional<TalkAction_t> action) const
    {
        const std::vector<TalkResult_t> results =
            action ? TalkScript::ResultsOf(*action) : std::vector{TalkResult_t::Done};
        std::map<TalkResult_t, TalkOutcome_t> outcomes;
        if (rChoice.Has("on") == rChoice.Has("then"))
        {
            rChoice.Fail("needs exactly one of 'then' and 'on'");
        }
        if (rChoice.Has("then"))
        {
            const TalkOutcome_t outcome = Outcome_(rChoice);
            for (const TalkResult_t result : results)
            {
                outcomes.emplace(result, outcome);
            }
            return outcomes;
        }
        const ConfigReader on = rChoice.Object("on");
        for (const auto& [rResult, rUnused] : on.Json().items())
        {
            const ConfigReader outcomeReader = on.Object(rResult);
            outcomeReader.RejectUnknownKeys({"say", "then"});
            const TalkResult_t result = EnumFromKey_<TalkResult_t>(on, rResult, "talk result");
            outcomes.emplace(result, Outcome_(outcomeReader));
        }
        for (const TalkResult_t result : results)
        {
            if (!outcomes.contains(result))
            {
                on.Fail("has no " + std::string(magic_enum::enum_name(result)));
            }
        }
        if (outcomes.size() != results.size())
        {
            on.Fail("names a result its action never returns");
        }
        return outcomes;
    }

    TalkOutcome_t Outcome_(const ConfigReader& rOutcome) const
    {
        TalkOutcome_t outcome{{}, rOutcome.String("then")};
        if (!rOutcome.Has("say"))
        {
            return outcome;
        }
        if (rOutcome.Json().at("say").is_array())
        {
            for (const ConfigReader& rRef : rOutcome.Objects("say"))
            {
                outcome.say.push_back(TextRef_(rRef));
            }
        }
        else
        {
            outcome.say.push_back(TextRef_(rOutcome.Object("say")));
        }
        if (outcome.say.empty())
        {
            rOutcome.FailKey("say", "lists no line");
        }
        return outcome;
    }

    TalkTextRef_t TextRef_(const ConfigReader& rRef) const
    {
        rRef.RejectUnknownKeys({"text", "item", "args", "extra"});
        TalkTextRef_t ref{rRef.String("text"), std::nullopt, {}};
        if (rRef.Has("item"))
        {
            ref.item = rRef.IntAtLeast("item", 0);
        }
        if (rRef.Has("args"))
        {
            ref.binding = NamedBinding_(rRef);
        }
        if (rRef.Has("extra"))
        {
            AddSlots_(ref.binding, rRef.Object("extra"));
        }
        return ref;
    }

    const TalkBinding_t& NamedBinding_(const ConfigReader& rRef) const
    {
        const std::string name = rRef.String("args");
        const auto it = m_bindings.find(name);
        if (it == m_bindings.end())
        {
            rRef.FailKey("args", "names no binding '" + name + "'");
        }
        return it->second;
    }

    ConfigReader m_reader;
    std::map<std::string, TalkBinding_t> m_bindings;
};

// Checks a parsed script against the text it names and the factions that will speak it.
class TalkScriptValidator_
{
public:
    TalkScriptValidator_(const TextPack& rText, std::string context)
        : m_rText(rText)
        , m_context(std::move(context))
    {
    }

    void Validate(const TalkPortrait_t& rPortrait, const TalkOutcome_t& rStart,
                  const std::map<std::string, TalkScene_t>& rScenes,
                  const FactionRegistry& rFactions)
    {
        m_pScenes = &rScenes;
        for (const int label : {rPortrait.mood, rPortrait.reputation, rPortrait.integrity})
        {
            (void)m_rText.Label(label);
        }
        Outcome_(rStart, "start");
        for (const auto& [rName, rScene] : rScenes)
        {
            Scene_(rScene, "scene '" + rName + "'");
        }
        Vocabularies_(rFactions);
    }

private:
    void Scene_(const TalkScene_t& rScene, const std::string& rContext)
    {
        Ref_(rScene.prompt, rContext + " prompt");
        if (rScene.draft)
        {
            Ref_(rScene.draft->offer, rContext + " draft");
            Ref_(rScene.draft->ask, rContext + " draft");
        }
        for (const TalkChoice_t& rChoice : rScene.choices)
        {
            std::visit([this, &rContext](const auto& rRow) { Choice_(rRow, rContext); }, rChoice);
        }
    }

    void Choice_(const TalkFixedChoice_t& rChoice, const std::string& rContext)
    {
        Ref_(rChoice.label, rContext + " label");
        for (const auto& [rResult, rOutcome] : rChoice.outcomes)
        {
            Outcome_(rOutcome, rContext);
        }
    }

    void Choice_(const TalkKindRows_t& rRows, const std::string& rContext)
    {
        RequireEveryKind_(rRows.labels, rContext);
        for (const auto& [rKind, rLabel] : rRows.labels)
        {
            Ref_(rLabel, rContext + " label");
        }
        SceneName_(rRows.then, rContext);
    }

    void Choice_(const TalkCandidateRows_t& rRows, const std::string& rContext)
    {
        RequireEveryKind_(rRows.labels, rContext);
        for (const auto& [rKind, rLabel] : rRows.labels)
        {
            CandidateRef_(rLabel, rContext + " label");
        }
        SceneName_(rRows.then, rContext);
    }

    // A new TradeItem_t alternative fails here until the script labels it.
    void RequireEveryKind_(const std::map<TradeKind_t, TalkTextRef_t>& rLabels,
                           const std::string& rContext) const
    {
        for (const TradeKind_t kind : TradeKinds())
        {
            if (!rLabels.contains(kind))
            {
                throw std::runtime_error(m_context + " " + rContext + ": no label for "
                                         + std::string(magic_enum::enum_name(kind)));
            }
        }
    }

    void Outcome_(const TalkOutcome_t& rOutcome, const std::string& rContext)
    {
        for (const TalkTextRef_t& rRef : rOutcome.say)
        {
            Ref_(rRef, rContext + " say");
        }
        SceneName_(rOutcome.then, rContext);
    }

    void SceneName_(const std::string& rName, const std::string& rContext) const
    {
        if (rName != TalkScript::k_EndScene && !m_pScenes->contains(rName))
        {
            throw std::runtime_error(m_context + " " + rContext + ": no scene '" + rName + "'");
        }
    }

    void Ref_(const TalkTextRef_t& rRef, const std::string& rContext)
    {
        const bool bCandidate = !rRef.binding.numbers.empty()
            || std::ranges::any_of(rRef.binding.strings,
                                   [](const auto& rSlot) { return IsCandidateSource_(rSlot.second); });
        if (bCandidate)
        {
            throw std::runtime_error(m_context + " " + rContext
                                     + ": candidate values exist only on trade rows");
        }
        CandidateRef_(rRef, rContext);
    }

    void CandidateRef_(const TalkTextRef_t& rRef, const std::string& rContext)
    {
        const std::string context = m_context + " " + rContext + " '" + rRef.id + "'";
        const ScriptSlots_t slots = SlotsOf(Template_(rRef, context));
        RequireBound_(slots.strings, rRef.binding.strings, context);
        RequireBound_(slots.numbers, rRef.binding.numbers, context);
        for (const auto& [rSlot, rSource] : rRef.binding.strings)
        {
            m_sources.push_back(rSource);
        }
    }

    const std::string& Template_(const TalkTextRef_t& rRef, const std::string& rContext) const
    {
        const ScriptEntry_t& rEntry = m_rText.Entry(rRef.id);
        if (!rRef.item)
        {
            return rEntry.body;
        }
        if (*rRef.item >= static_cast<int>(rEntry.items.size()))
        {
            throw std::runtime_error(rContext + ": no item " + std::to_string(*rRef.item));
        }
        return rEntry.items[static_cast<std::size_t>(*rRef.item)];
    }

    static void RequireBound_(const std::set<int>& rRead,
                              const std::map<int, TalkValueSource_t>& rBound,
                              const std::string& rContext)
    {
        for (const int slot : rRead)
        {
            if (!rBound.contains(slot))
            {
                throw std::runtime_error(rContext + ": slot " + std::to_string(slot)
                                         + " is read but not bound");
            }
        }
    }

    // Every faction that talks must have every field and phrase the bindings read.
    void Vocabularies_(const FactionRegistry& rFactions) const
    {
        for (const FactionConfig_t& rFaction : rFactions.GetAll())
        {
            if (IsNativeLifeFaction(rFaction.identity.species))
            {
                continue;
            }
            const FactionVocabulary_t& rVocabulary = m_rText.Vocabulary(rFaction.id);
            for (const TalkValueSource_t& rSource : m_sources)
            {
                Source_(rSource, rVocabulary, rFaction.id);
            }
        }
    }

    void Source_(const TalkValueSource_t& rSource, const FactionVocabulary_t& rVocabulary,
                 const std::string& rFactionId) const
    {
        const std::string context = m_context + ": faction '" + rFactionId + "'";
        const bool bPhrase = rSource.kind == TalkValueKind_t::Phrase
            || rSource.kind == TalkValueKind_t::NounPhrase;
        if (rSource.kind == TalkValueKind_t::Field && !rVocabulary.fields.contains(rSource.key))
        {
            throw std::runtime_error(context + " has no field '" + rSource.key + "'");
        }
        if (bPhrase && !rVocabulary.phrases.contains(rSource.key))
        {
            throw std::runtime_error(context + " has no phrase '" + rSource.key + "'");
        }
    }

    const TextPack& m_rText;
    std::string m_context;
    const std::map<std::string, TalkScene_t>* m_pScenes = nullptr;
    std::vector<TalkValueSource_t> m_sources;
};

} // namespace

TalkScript::TalkScript(TalkPortrait_t portrait, std::vector<int> creditOffers, TalkOutcome_t start,
                       std::map<std::string, TalkScene_t> scenes)
    : m_portrait(portrait)
    , m_creditOffers(std::move(creditOffers))
    , m_start(std::move(start))
    , m_scenes(std::move(scenes))
{
}

TalkScript TalkScript::Load(const std::string& rPath, const TextPack& rText,
                            const FactionRegistry& rFactions)
{
    return JsonConfigLoader::LoadObjectFile<TalkScript>(
        rPath, "talk script",
        [&](const ConfigReader& rReader) { return Parse_(rReader, rText, rFactions); });
}

TalkScript TalkScript::Parse_(const ConfigReader& rReader, const TextPack& rText,
                              const FactionRegistry& rFactions)
{
    const TalkScriptParser_ parser(rReader);
    TalkScript script(parser.Portrait(), parser.CreditOffers(), parser.Start(), parser.Scenes());
    TalkScriptValidator_(rText, rReader.Context())
        .Validate(script.m_portrait, script.m_start, script.m_scenes, rFactions);
    return script;
}

const TalkScene_t& TalkScript::Scene(const std::string& rName) const
{
    const auto it = m_scenes.find(rName);
    if (it == m_scenes.end())
    {
        throw std::runtime_error("TalkScript: no scene '" + rName + "'");
    }
    return it->second;
}

std::vector<TalkResult_t> TalkScript::ResultsOf(TalkAction_t action)
{
    switch (action)
    {
    case TalkAction_t::ProposeTruce:
    case TalkAction_t::ProposeTreaty:
    case TalkAction_t::ProposePact:
    case TalkAction_t::ProposeTrade:
        return {TalkResult_t::Accepted, TalkResult_t::Rejected, TalkResult_t::Invalid};
    case TalkAction_t::RenouncePact:
    case TalkAction_t::ClearTrade:
        return {TalkResult_t::Done};
    }
    throw std::logic_error("TalkScript::ResultsOf: unhandled TalkAction_t");
}

} // namespace ac
