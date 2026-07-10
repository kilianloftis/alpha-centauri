#include "game/faction/FactionConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "game/effects/BonusEffectParser.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

namespace
{

constexpr const char* kIdentityFile = "identity.json";
constexpr const char* kLeaderFile = "leader.json";
constexpr const char* kAIFile = "ai.json";
constexpr const char* kEffectsFile = "effects.json";
constexpr const char* kBaseNamesFile = "base_names.json";
constexpr const char* kPhrasesFile = "phrases.json";

} // namespace

std::vector<FactionConfig_t> FactionConfigParser::ParseConfig(const std::string& configPath)
{
    namespace fs = std::filesystem;

    std::cout << "Loading faction configuration from: " << configPath << "\n";

    if (!fs::exists(configPath))
    {
        throw std::runtime_error("Faction config path does not exist: " + configPath);
    }

    std::vector<FactionConfig_t> configs;

    if (fs::is_directory(configPath))
    {
        std::vector<fs::path> factionDirs;
        for (const auto& rEntry : fs::directory_iterator(configPath))
        {
            if (rEntry.is_directory())
            {
                factionDirs.push_back(rEntry.path());
            }
        }
        std::sort(factionDirs.begin(), factionDirs.end());

        for (const auto& rDir : factionDirs)
        {
            configs.push_back(ParseFactionDirectory(rDir.filename().string(), rDir.string()));
        }
    }
    else
    {
        throw std::runtime_error("Faction config path must be a directory: " + configPath);
    }

    std::cout << "Loaded " << configs.size() << " faction configurations\n";
    return configs;
}

FactionConfig_t FactionConfigParser::ParseFactionDirectory(const std::string& id,
                                                           const std::string& dirPath)
{
    FactionConfig_t config;
    config.id = id;

    config.identity = ParseIdentity(ReadRequiredJsonFile(dirPath + "/" + kIdentityFile), id);
    config.leader = ParseLeader(ReadRequiredJsonFile(dirPath + "/" + kLeaderFile));
    config.ai = ParseAITendencies(ReadRequiredJsonFile(dirPath + "/" + kAIFile));
    config.effects = BonusEffectParser::ParseEffects(
        ReadRequiredJsonFile(dirPath + "/" + kEffectsFile), EffectSourceKind::Faction, config.id);
    config.flavor.baseNames =
        ParseBaseNames(ReadRequiredJsonFile(dirPath + "/" + kBaseNamesFile));
    config.flavor.phrases = ParsePhrases(ReadRequiredJsonFile(dirPath + "/" + kPhrasesFile));

    return config;
}

FactionIdentityConfig FactionConfigParser::ParseIdentity(const nlohmann::json& j,
                                                         const std::string& idFallback)
{
    FactionIdentityConfig identity;
    identity.name = ConfigFields::ParseName(j, idFallback);
    identity.descriptiveName = j.value("descriptive_name", identity.name);
    identity.noun = j.value("noun", identity.name);
    identity.adjective = j.value("adjective", identity.name);
    return identity;
}

LeaderConfig FactionConfigParser::ParseLeader(const nlohmann::json& j)
{
    LeaderConfig leader;
    leader.name = j.at("name").get<std::string>();
    leader.title = j.value("title", std::string());
    return leader;
}

AITendenciesConfig FactionConfigParser::ParseAITendencies(const nlohmann::json& j)
{
    AITendenciesConfig ai;
    ai.wealth = j.value("wealth", false);
    ai.power = j.value("power", false);
    ai.growth = j.value("growth", false);
    ai.tech = j.value("tech", false);
    return ai;
}

std::vector<std::string> FactionConfigParser::ParseBaseNames(const nlohmann::json& j)
{
    if (!j.is_array())
    {
        throw std::runtime_error("base_names.json must be a JSON array of strings");
    }

    std::vector<std::string> names;
    names.reserve(j.size());
    for (const auto& rEntry : j)
    {
        names.push_back(rEntry.get<std::string>());
    }
    return names;
}

std::unordered_map<std::string, std::vector<std::string>> FactionConfigParser::ParsePhrases(
    const nlohmann::json& j)
{
    if (!j.is_object())
    {
        throw std::runtime_error("phrases.json must be a JSON object of string arrays");
    }

    std::unordered_map<std::string, std::vector<std::string>> phrases;
    for (const auto& [key, value] : j.items())
    {
        if (!value.is_array())
        {
            throw std::runtime_error("phrases.json entry '" + key + "' must be a string array");
        }
        std::vector<std::string> lines;
        lines.reserve(value.size());
        for (const auto& rLine : value)
        {
            lines.push_back(rLine.get<std::string>());
        }
        phrases.emplace(key, std::move(lines));
    }
    return phrases;
}

nlohmann::json FactionConfigParser::ReadJsonFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    nlohmann::json json;
    file >> json;
    return json;
}

nlohmann::json FactionConfigParser::ReadRequiredJsonFile(const std::string& filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("Required faction config file missing: " + filePath);
    }
    return ReadJsonFile(filePath);
}

} // namespace ac
