#include "ui/text/TextPack.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{

// Whose grammar a header field carries when it fills a slot.
enum class FieldGrammar_t
{
    Leader,
    FactionNoun
};

struct VocabularyField_t
{
    const char* key;
    FieldGrammar_t grammar;
};

constexpr VocabularyField_t k_VocabularyFields[] = {
    {"name_leader", FieldGrammar_t::Leader},
    {"title_leader", FieldGrammar_t::Leader},
    {"adj_leader", FieldGrammar_t::Leader},
    {"adj_insult_leader", FieldGrammar_t::Leader},
    {"insult_leader", FieldGrammar_t::Leader},
    {"formal_name_faction", FieldGrammar_t::FactionNoun},
    {"desc_name_faction", FieldGrammar_t::FactionNoun},
    {"noun_faction", FieldGrammar_t::FactionNoun},
    {"adj_name_faction", FieldGrammar_t::FactionNoun},
    {"adj_faction", FieldGrammar_t::FactionNoun},
    {"adj_insult_faction", FieldGrammar_t::FactionNoun},
};

// The phrase lines in file order, named for the sentences they complete (terranx-map.md, Talk).
constexpr const char* k_PhraseKeys[] = {
    "goal",
    "intent",
    "agenda",
    "your_agenda",
    "misdeed",
    "pastime",
    "your_pastime",
    "accusation",
    "methods",
    "projects",
    "pursuits",
    "tribute",
    "service",
    "enforcers",
    "code",
};

const std::string& RequireString_(const nlohmann::json& rJson, const std::string& rKey,
                                  const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_string())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a string");
    }
    return rJson.at(rKey).get_ref<const std::string&>();
}

bool RequireBool_(const nlohmann::json& rJson, const std::string& rKey,
                  const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_boolean())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a boolean");
    }
    return rJson.at(rKey).get<bool>();
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

std::vector<std::string> RequireStrings_(const nlohmann::json& rJson, const std::string& rKey,
                                         const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_array())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be an array of strings");
    }
    std::vector<std::string> strings;
    for (const nlohmann::json& rString : rJson.at(rKey))
    {
        if (!rString.is_string())
        {
            throw std::runtime_error(rContext + ": '" + rKey + "' must be an array of strings");
        }
        strings.push_back(rString.get<std::string>());
    }
    return strings;
}

GrammaticalGender_t ParseGender_(const std::string& rCode, const std::string& rContext)
{
    if (rCode == "M")
    {
        return GrammaticalGender_t::Masculine;
    }
    if (rCode == "F")
    {
        return GrammaticalGender_t::Feminine;
    }
    if (rCode == "N")
    {
        return GrammaticalGender_t::Neuter;
    }
    throw std::runtime_error(rContext + ": 'noun_gender' must be \"M\", \"F\" or \"N\"");
}

} // namespace

TextPack TextPack::Load(const std::vector<std::string>& rRoots, const std::string& rLocale)
{
    TextPack pack;
    for (const std::string& rRoot : rRoots)
    {
        const std::filesystem::path directory = std::filesystem::path(rRoot) / rLocale;
        if (std::filesystem::is_directory(directory))
        {
            pack.LoadLocaleDirectory_(directory);
        }
    }
    return pack;
}

const ScriptEntry_t& TextPack::Entry(const std::string& rId) const
{
    const auto it = m_entries.find(rId);
    if (it == m_entries.end())
    {
        throw std::runtime_error("TextPack: no script entry '" + rId + "'");
    }
    return it->second;
}

bool TextPack::HasEntry(const std::string& rId) const
{
    return m_entries.find(rId) != m_entries.end();
}

const std::string& TextPack::Label(int label) const
{
    const auto it = m_labels.find(label);
    if (it == m_labels.end())
    {
        throw std::runtime_error("TextPack: no label " + std::to_string(label));
    }
    return it->second;
}

const FactionVocabulary_t& TextPack::Vocabulary(const std::string& rFactionId) const
{
    const auto it = m_vocabularies.find(rFactionId);
    if (it == m_vocabularies.end())
    {
        throw std::runtime_error("TextPack: no vocabulary for faction '" + rFactionId + "'");
    }
    return it->second;
}

void TextPack::LoadLocaleDirectory_(const std::filesystem::path& rDirectory)
{
    if (std::filesystem::is_regular_file(rDirectory / "script.json"))
    {
        LoadScript_(rDirectory / "script.json");
    }
    if (std::filesystem::is_regular_file(rDirectory / "labels.json"))
    {
        LoadLabels_(rDirectory / "labels.json");
    }
    if (std::filesystem::is_directory(rDirectory / "factions"))
    {
        LoadVocabularies_(rDirectory / "factions");
    }
}

void TextPack::LoadScript_(const std::filesystem::path& rPath)
{
    const std::string context = "Script '" + rPath.string() + "'";
    std::map<std::string, ScriptEntry_t> entries =
        JsonConfigLoader::LoadObjectFile<std::map<std::string, ScriptEntry_t>>(
            rPath.string(), "script text",
            [&context](const ConfigReader& rReader) {
                return ParseScript_(rReader.Json(), context);
            });
    m_entries.merge(entries);
}

void TextPack::LoadLabels_(const std::filesystem::path& rPath)
{
    const std::string context = "Labels '" + rPath.string() + "'";
    std::map<int, std::string> labels = JsonConfigLoader::LoadObjectFile<std::map<int, std::string>>(
        rPath.string(), "label text",
        [&context](const ConfigReader& rReader) {
            return ParseLabels_(rReader.Json(), context);
        });
    m_labels.merge(labels);
}

void TextPack::LoadVocabularies_(const std::filesystem::path& rDirectory)
{
    std::vector<std::filesystem::path> files;
    for (const auto& rFile : std::filesystem::directory_iterator(rDirectory))
    {
        if (rFile.path().extension() == ".json")
        {
            files.push_back(rFile.path());
        }
    }
    std::ranges::sort(files);
    for (const std::filesystem::path& rFile : files)
    {
        const std::string context = "Faction vocabulary '" + rFile.string() + "'";
        m_vocabularies.emplace(
            rFile.stem().string(),
            JsonConfigLoader::LoadObjectFile<FactionVocabulary_t>(
                rFile.string(), "faction vocabulary",
                [&context](const ConfigReader& rReader) {
                    return ParseVocabulary_(rReader.Json(), context);
                }));
    }
}

std::map<std::string, ScriptEntry_t> TextPack::ParseScript_(const nlohmann::json& rJson,
                                                            const std::string& rContext)
{
    std::map<std::string, ScriptEntry_t> entries;
    for (const auto& [rId, rEntry] : rJson.items())
    {
        entries.emplace(rId, ParseEntry_(rEntry, rContext + " '" + rId + "'"));
    }
    return entries;
}

std::map<int, std::string> TextPack::ParseLabels_(const nlohmann::json& rJson,
                                                  const std::string& rContext)
{
    std::map<int, std::string> labels;
    for (const auto& [rKey, rLabel] : rJson.items())
    {
        const bool bNumber = !rKey.empty()
            && std::ranges::all_of(rKey, [](char c) { return c >= '0' && c <= '9'; });
        if (!bNumber || !rLabel.is_string())
        {
            throw std::runtime_error(rContext + ": '" + rKey
                                     + "' must be a label number with a string");
        }
        labels.emplace(std::stoi(rKey), rLabel.get<std::string>());
    }
    return labels;
}

ScriptEntry_t TextPack::ParseEntry_(const nlohmann::json& rJson, const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + ": must be an object");
    }
    ConfigFields::RejectUnknownKeys(rJson, {"body", "items"}, rContext);
    return ScriptEntry_t{RequireString_(rJson, "body", rContext),
                         RequireStrings_(rJson, "items", rContext)};
}

FactionVocabulary_t TextPack::ParseVocabulary_(const nlohmann::json& rJson,
                                               const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(
        rJson, {"leader_female", "noun_gender", "noun_plural", "fields", "phrases"}, rContext);
    const GrammaticalGender_t leaderGender = RequireBool_(rJson, "leader_female", rContext)
        ? GrammaticalGender_t::Feminine
        : GrammaticalGender_t::Masculine;
    const GrammaticalGender_t nounGender =
        ParseGender_(RequireString_(rJson, "noun_gender", rContext), rContext);
    const bool bNounPlural = RequireBool_(rJson, "noun_plural", rContext);

    const nlohmann::json& rFields = RequireObject_(rJson, "fields", rContext);
    std::vector<std::string> known;
    FactionVocabulary_t vocabulary;
    for (const VocabularyField_t& rField : k_VocabularyFields)
    {
        known.emplace_back(rField.key);
        const bool bLeader = rField.grammar == FieldGrammar_t::Leader;
        const std::string& rText = RequireString_(rFields, rField.key, rContext + " 'fields'");
        vocabulary.fields.emplace(
            rField.key,
            ScriptValue_t{rText, bLeader ? leaderGender : nounGender, !bLeader && bNounPlural});
    }
    ConfigFields::RejectUnknownKeys(rFields, known, rContext + " 'fields'");

    const nlohmann::json& rPhrases = RequireObject_(rJson, "phrases", rContext);
    const std::vector<std::string> phraseKeys(std::begin(k_PhraseKeys), std::end(k_PhraseKeys));
    for (const std::string& rKey : phraseKeys)
    {
        vocabulary.phrases.emplace(rKey, RequireString_(rPhrases, rKey, rContext + " 'phrases'"));
    }
    ConfigFields::RejectUnknownKeys(rPhrases, phraseKeys, rContext + " 'phrases'");
    return vocabulary;
}

} // namespace ac
