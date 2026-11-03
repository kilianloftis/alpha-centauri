#pragma once

#include "ui/text/ScriptMarkup.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ac
{

// One script.json entry: the lines up to the first blank line, and the item lines after it.
struct ScriptEntry_t
{
    std::string body;
    std::vector<std::string> items;
};

// One faction's talk words (factions/<id>.json): header fields under SMAC's MFaction names, each
// with the grammar slots agree with, and the phrase lines get_phrase indexes, named for the
// sentence each completes (terranx-map.md, Talk).
struct FactionVocabulary_t
{
    std::map<std::string, ScriptValue_t> fields;
    std::map<std::string, std::string> phrases;
};

// Player-facing text for one locale, from <root>/<locale>/ under each root in lookup order. Each
// script entry and label comes from the first root that has it, each faction file whole from the
// first root that has it. A root without the locale is skipped.
class TextPack
{
public:
    static TextPack Load(const std::vector<std::string>& rRoots, const std::string& rLocale);

    // Each throws for an id no root has.
    const ScriptEntry_t& Entry(const std::string& rId) const;
    const std::string& Label(int label) const;
    const FactionVocabulary_t& Vocabulary(const std::string& rFactionId) const;

private:
    TextPack() = default;

    void LoadLocaleDirectory_(const std::filesystem::path& rDirectory);
    void LoadScript_(const std::filesystem::path& rPath);
    void LoadLabels_(const std::filesystem::path& rPath);
    void LoadVocabularies_(const std::filesystem::path& rDirectory);
    static std::map<std::string, ScriptEntry_t> ParseScript_(const nlohmann::json& rJson,
                                                             const std::string& rContext);
    static std::map<int, std::string> ParseLabels_(const nlohmann::json& rJson,
                                                   const std::string& rContext);
    static ScriptEntry_t ParseEntry_(const nlohmann::json& rJson, const std::string& rContext);
    static FactionVocabulary_t ParseVocabulary_(const nlohmann::json& rJson,
                                                const std::string& rContext);

    std::map<std::string, ScriptEntry_t> m_entries;
    std::map<int, std::string> m_labels;
    std::map<std::string, FactionVocabulary_t> m_vocabularies;
};

} // namespace ac
