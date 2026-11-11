#include "ui/text/TextPack.h"

#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
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

std::vector<std::string> RequireStrings_(const ConfigReader& rReader, const char* pKey)
{
    rReader.Array(pKey);
    return rReader.StringArray(pKey);
}

GrammaticalGender_t ParseGender_(const ConfigReader& rReader)
{
    const std::string code = rReader.String("noun_gender");
    if (code == "M")
    {
        return GrammaticalGender_t::Masculine;
    }
    if (code == "F")
    {
        return GrammaticalGender_t::Feminine;
    }
    if (code == "N")
    {
        return GrammaticalGender_t::Neuter;
    }
    rReader.FailKey("noun_gender", "must be \"M\", \"F\" or \"N\"");
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
    std::map<std::string, ScriptEntry_t> entries =
        JsonConfigLoader::LoadObjectFile<std::map<std::string, ScriptEntry_t>>(
            rPath.string(), "script text", ParseScript_);
    m_entries.merge(entries);
}

void TextPack::LoadLabels_(const std::filesystem::path& rPath)
{
    std::map<int, std::string> labels = JsonConfigLoader::LoadObjectFile<std::map<int, std::string>>(
        rPath.string(), "label text", ParseLabels_);
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
        m_vocabularies.emplace(rFile.stem().string(),
                               JsonConfigLoader::LoadObjectFile<FactionVocabulary_t>(
                                   rFile.string(), "faction vocabulary", ParseVocabulary_));
    }
}

std::map<std::string, ScriptEntry_t> TextPack::ParseScript_(const ConfigReader& rReader)
{
    std::map<std::string, ScriptEntry_t> entries;
    for (const auto& [rId, rUnused] : rReader.Json().items())
    {
        entries.emplace(rId, ParseEntry_(rReader.Object(rId)));
    }
    return entries;
}

std::map<int, std::string> TextPack::ParseLabels_(const ConfigReader& rReader)
{
    std::map<int, std::string> labels;
    for (const auto& [rKey, rLabel] : rReader.Json().items())
    {
        const bool bNumber = !rKey.empty()
            && std::ranges::all_of(rKey, [](char c) { return c >= '0' && c <= '9'; });
        if (!bNumber)
        {
            rReader.FailKey(rKey, "must be a label number");
        }
        labels.emplace(std::stoi(rKey), rReader.String(rKey));
    }
    return labels;
}

ScriptEntry_t TextPack::ParseEntry_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"body", "items"});
    return ScriptEntry_t{rReader.String("body"), RequireStrings_(rReader, "items")};
}

FactionVocabulary_t TextPack::ParseVocabulary_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"leader_female", "noun_gender", "noun_plural", "fields", "phrases"});
    const GrammaticalGender_t leaderGender = rReader.Bool("leader_female")
        ? GrammaticalGender_t::Feminine
        : GrammaticalGender_t::Masculine;
    const GrammaticalGender_t nounGender = ParseGender_(rReader);
    const bool bNounPlural = rReader.Bool("noun_plural");

    const ConfigReader fields = rReader.Object("fields");
    std::vector<std::string> known;
    FactionVocabulary_t vocabulary;
    for (const VocabularyField_t& rField : k_VocabularyFields)
    {
        known.emplace_back(rField.key);
        const bool bLeader = rField.grammar == FieldGrammar_t::Leader;
        vocabulary.fields.emplace(rField.key,
                                  ScriptValue_t{fields.String(rField.key),
                                                bLeader ? leaderGender : nounGender,
                                                !bLeader && bNounPlural});
    }
    fields.RejectUnknownKeys(known);

    const ConfigReader phrases = rReader.Object("phrases");
    const std::vector<std::string> phraseKeys(std::begin(k_PhraseKeys), std::end(k_PhraseKeys));
    for (const std::string& rKey : phraseKeys)
    {
        vocabulary.phrases.emplace(rKey, phrases.String(rKey));
    }
    phrases.RejectUnknownKeys(phraseKeys);
    return vocabulary;
}

} // namespace ac
