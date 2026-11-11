#include "ui/commlinks/TalkScript.h"

#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "lib/config/ConfigFields.h"
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

template <typename Enum>
Enum ParseEnum_(const nlohmann::json& rJson, const std::string& rContext)
{
    const std::optional<Enum> value =
        rJson.is_string() ? magic_enum::enum_cast<Enum>(rJson.get<std::string>()) : std::nullopt;
    if (!value)
    {
        throw std::runtime_error(rContext + ": unknown " + std::string(magic_enum::enum_type_name<Enum>())
                                 + " " + rJson.dump());
    }
    return *value;
}

const nlohmann::json& RequireObject_(const nlohmann::json& rJson, const std::string& rKey,
                                     const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_object())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be an object");
    }
    return rJson.at(rKey);
}

const std::string& RequireString_(const nlohmann::json& rJson, const std::string& rKey,
                                  const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_string())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a string");
    }
    return rJson.at(rKey).get_ref<const std::string&>();
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
void AddSlots_(TalkBinding_t& rBinding, const nlohmann::json& rJson, const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + ": a binding must be an object");
    }
    for (const auto& [rKey, rSource] : rJson.items())
    {
        if (!rSource.is_string())
        {
            throw std::runtime_error(rContext + ": slot '" + rKey + "' must name a value source");
        }
        const bool bNumber = rKey.starts_with('#');
        const int slot = ParseIndex_(bNumber ? rKey.substr(1) : rKey, rContext);
        if (slot > 9)
        {
            throw std::runtime_error(rContext + ": slot '" + rKey + "' is past slot 9");
        }
        const TalkValueSource_t source = ParseSource_(rSource.get<std::string>(), rContext);
        if (IsNumberSource_(source) != bNumber)
        {
            throw std::runtime_error(rContext + ": slot '" + rKey + "' and '"
                                     + rSource.get<std::string>() + "' disagree on number");
        }
        (bNumber ? rBinding.numbers : rBinding.strings)[slot] = source;
    }
}

class TalkScriptParser_
{
public:
    TalkScriptParser_(const nlohmann::json& rJson, std::string context)
        : m_rJson(rJson)
        , m_context(std::move(context))
    {
        ConfigFields::RejectUnknownKeys(
            rJson, {"portrait", "credit_offers", "bindings", "start", "scenes"}, m_context);
        for (const auto& [rName, rSlots] : RequireObject_(rJson, "bindings", m_context).items())
        {
            AddSlots_(m_bindings[rName], rSlots, m_context + " binding '" + rName + "'");
        }
    }

    TalkPortrait_t Portrait() const
    {
        const nlohmann::json& rPortrait = RequireObject_(m_rJson, "portrait", m_context);
        const std::string context = m_context + " 'portrait'";
        ConfigFields::RejectUnknownKeys(rPortrait, {"mood", "reputation", "integrity"}, context);
        return TalkPortrait_t{ConfigFields::RequireIntAtLeast(rPortrait, "mood", 0, context),
                              ConfigFields::RequireIntAtLeast(rPortrait, "reputation", 0, context),
                              ConfigFields::RequireIntAtLeast(rPortrait, "integrity", 0, context)};
    }

    std::vector<int> CreditOffers() const
    {
        const std::string context = m_context + " 'credit_offers'";
        if (!m_rJson.contains("credit_offers") || !m_rJson.at("credit_offers").is_array()
            || m_rJson.at("credit_offers").empty())
        {
            throw std::runtime_error(context + ": must be a non-empty array");
        }
        std::vector<int> offers;
        for (const nlohmann::json& rOffer : m_rJson.at("credit_offers"))
        {
            if (!rOffer.is_number_integer() || rOffer.get<int>() <= 0)
            {
                throw std::runtime_error(context + ": each offer must be a positive integer");
            }
            offers.push_back(rOffer.get<int>());
        }
        return offers;
    }

    TalkOutcome_t Start() const
    {
        const nlohmann::json& rStart = RequireObject_(m_rJson, "start", m_context);
        const std::string context = m_context + " 'start'";
        ConfigFields::RejectUnknownKeys(rStart, {"say", "then"}, context);
        return Outcome_(rStart, context);
    }

    std::map<std::string, TalkScene_t> Scenes() const
    {
        std::map<std::string, TalkScene_t> scenes;
        for (const auto& [rName, rScene] : RequireObject_(m_rJson, "scenes", m_context).items())
        {
            scenes.emplace(rName, Scene_(rScene, m_context + " scene '" + rName + "'"));
        }
        return scenes;
    }

private:
    TalkScene_t Scene_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        ConfigFields::RejectUnknownKeys(rJson, {"prompt", "choices", "draft"}, rContext);
        const nlohmann::json& rPrompt = RequireObject_(rJson, "prompt", rContext);
        TalkScene_t scene{TextRef_(rPrompt, rContext + " prompt"), {}, std::nullopt};
        if (!rJson.contains("choices") || !rJson.at("choices").is_array())
        {
            throw std::runtime_error(rContext + ": 'choices' must be an array");
        }
        for (const nlohmann::json& rChoice : rJson.at("choices"))
        {
            scene.choices.push_back(Choice_(rChoice, rContext + " choice"));
        }
        if (rJson.contains("draft"))
        {
            scene.draft = Draft_(rJson.at("draft"), rContext + " draft");
        }
        return scene;
    }

    TalkDraftLabels_t Draft_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        ConfigFields::RejectUnknownKeys(rJson, {"offer", "ask"}, rContext);
        const nlohmann::json& rOffer = RequireObject_(rJson, "offer", rContext);
        const nlohmann::json& rAsk = RequireObject_(rJson, "ask", rContext);
        return TalkDraftLabels_t{TextRef_(rOffer, rContext), TextRef_(rAsk, rContext)};
    }

    TalkChoice_t Choice_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        if (!rJson.is_object())
        {
            throw std::runtime_error(rContext + ": must be an object");
        }
        if (!rJson.contains("from"))
        {
            return FixedChoice_(rJson, rContext);
        }
        const std::string& rFrom = RequireString_(rJson, "from", rContext);
        if (rFrom == "trade_kinds")
        {
            ConfigFields::RejectUnknownKeys(rJson, {"from", "side", "labels", "then"}, rContext);
            const TradeSide_t side = ParseEnum_<TradeSide_t>(rJson.at("side"), rContext);
            return TalkKindRows_t{side, KindLabels_(rJson, rContext),
                                  RequireString_(rJson, "then", rContext)};
        }
        if (rFrom == "trade_candidates")
        {
            ConfigFields::RejectUnknownKeys(rJson, {"from", "labels", "then"}, rContext);
            return TalkCandidateRows_t{KindLabels_(rJson, rContext),
                                       RequireString_(rJson, "then", rContext)};
        }
        throw std::runtime_error(rContext + ": unknown generator '" + rFrom + "'");
    }

    std::map<TradeKind_t, TalkTextRef_t> KindLabels_(const nlohmann::json& rJson,
                                                    const std::string& rContext) const
    {
        std::map<TradeKind_t, TalkTextRef_t> labels;
        for (const auto& [rKind, rLabel] : RequireObject_(rJson, "labels", rContext).items())
        {
            const nlohmann::json kindName(rKind);
            const TradeKind_t kind = ParseEnum_<TradeKind_t>(kindName, rContext);
            labels.emplace(kind, TextRef_(rLabel, rContext + " label '" + rKind + "'"));
        }
        return labels;
    }

    TalkFixedChoice_t FixedChoice_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        ConfigFields::RejectUnknownKeys(rJson, {"label", "when", "do", "say", "then", "on"},
                                        rContext);
        const nlohmann::json& rLabel = RequireObject_(rJson, "label", rContext);
        TalkFixedChoice_t choice{TextRef_(rLabel, rContext + " label"), std::nullopt,
                                 std::nullopt, {}};
        if (rJson.contains("when"))
        {
            choice.when = ParseEnum_<DiplomaticActionKind_t>(rJson.at("when"), rContext);
        }
        if (rJson.contains("do"))
        {
            choice.action = ParseEnum_<TalkAction_t>(rJson.at("do"), rContext);
        }
        choice.outcomes = Outcomes_(rJson, choice.action, rContext);
        return choice;
    }

    // `then` sends every result one way; `on` gives each result its own.
    std::map<TalkResult_t, TalkOutcome_t> Outcomes_(const nlohmann::json& rJson,
                                                    std::optional<TalkAction_t> action,
                                                    const std::string& rContext) const
    {
        const std::vector<TalkResult_t> results =
            action ? TalkScript::ResultsOf(*action) : std::vector{TalkResult_t::Done};
        std::map<TalkResult_t, TalkOutcome_t> outcomes;
        if (rJson.contains("on") == rJson.contains("then"))
        {
            throw std::runtime_error(rContext + ": needs exactly one of 'then' and 'on'");
        }
        if (rJson.contains("then"))
        {
            const TalkOutcome_t outcome = Outcome_(rJson, rContext);
            for (const TalkResult_t result : results)
            {
                outcomes.emplace(result, outcome);
            }
            return outcomes;
        }
        for (const auto& [rResult, rOutcome] : RequireObject_(rJson, "on", rContext).items())
        {
            const std::string context = rContext + " on '" + rResult + "'";
            ConfigFields::RejectUnknownKeys(rOutcome, {"say", "then"}, context);
            const nlohmann::json resultName(rResult);
            const TalkResult_t result = ParseEnum_<TalkResult_t>(resultName, context);
            outcomes.emplace(result, Outcome_(rOutcome, context));
        }
        for (const TalkResult_t result : results)
        {
            if (!outcomes.contains(result))
            {
                throw std::runtime_error(rContext + ": 'on' has no "
                                         + std::string(magic_enum::enum_name(result)));
            }
        }
        if (outcomes.size() != results.size())
        {
            throw std::runtime_error(rContext + ": 'on' names a result its action never returns");
        }
        return outcomes;
    }

    TalkOutcome_t Outcome_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        TalkOutcome_t outcome{{}, RequireString_(rJson, "then", rContext)};
        if (!rJson.contains("say"))
        {
            return outcome;
        }
        const nlohmann::json& rSay = rJson.at("say");
        if (rSay.is_array())
        {
            for (const nlohmann::json& rRef : rSay)
            {
                outcome.say.push_back(TextRef_(rRef, rContext + " say"));
            }
        }
        else
        {
            outcome.say.push_back(TextRef_(rSay, rContext + " say"));
        }
        if (outcome.say.empty())
        {
            throw std::runtime_error(rContext + ": 'say' lists no line");
        }
        return outcome;
    }

    TalkTextRef_t TextRef_(const nlohmann::json& rJson, const std::string& rContext) const
    {
        if (!rJson.is_object())
        {
            throw std::runtime_error(rContext + ": a text reference must be an object");
        }
        ConfigFields::RejectUnknownKeys(rJson, {"text", "item", "args", "extra"}, rContext);
        TalkTextRef_t ref{RequireString_(rJson, "text", rContext), std::nullopt, {}};
        if (rJson.contains("item"))
        {
            ref.item = ConfigFields::RequireIntAtLeast(rJson, "item", 0, rContext);
        }
        if (rJson.contains("args"))
        {
            ref.binding = NamedBinding_(RequireString_(rJson, "args", rContext), rContext);
        }
        if (rJson.contains("extra"))
        {
            AddSlots_(ref.binding, rJson.at("extra"), rContext + " extra");
        }
        return ref;
    }

    const TalkBinding_t& NamedBinding_(const std::string& rName, const std::string& rContext) const
    {
        const auto it = m_bindings.find(rName);
        if (it == m_bindings.end())
        {
            throw std::runtime_error(rContext + ": no binding '" + rName + "'");
        }
        return it->second;
    }

    const nlohmann::json& m_rJson;
    std::string m_context;
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
        [&](const ConfigReader& rReader) {
            return Parse_(rReader.Json(), rPath, rText, rFactions);
        });
}

TalkScript TalkScript::Parse_(const nlohmann::json& rJson, const std::string& rPath,
                              const TextPack& rText, const FactionRegistry& rFactions)
{
    const std::string context = "Talk script '" + rPath + "'";
    const TalkScriptParser_ parser(rJson, context);
    TalkScript script(parser.Portrait(), parser.CreditOffers(), parser.Start(), parser.Scenes());
    TalkScriptValidator_(rText, context)
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
