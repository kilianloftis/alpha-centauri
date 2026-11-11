#include "game/faction/FactionConfigParser.h"

#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace ac
{

namespace
{

constexpr const char* k_IdentityFile = "identity.json";
constexpr const char* k_LeaderFile = "leader.json";
constexpr const char* k_AIFile = "ai.json";
constexpr const char* k_EffectsFile = "effects.json";
constexpr const char* k_BaseNamesFile = "base_names.json";

FactionSpecies_t ParseFactionSpecies_(const ConfigReader& rReader)
{
    const std::string value = rReader.String("species");
    if (value == "human")       return FactionSpecies_t::Human;
    if (value == "progenitor")  return FactionSpecies_t::Progenitor;
    if (value == "native_life") return FactionSpecies_t::NativeLife;
    rReader.FailKey("species", "is not a known species: '" + value + "'");
}

// One file of a faction's directory. The json lives for the whole parse.
template <typename T, typename ParseFn>
T ParseFactionFile_(const std::string& rPath, ParseFn parse)
{
    const nlohmann::json json = JsonConfigLoader::ReadFile(rPath);
    return parse(ConfigReader(json, "faction file '" + rPath + "'"));
}

FactionIdentityConfig ParseIdentity_(const ConfigReader& rReader)
{
    FactionIdentityConfig identity;
    identity.name = rReader.String("name");
    identity.descriptiveName = rReader.StringOr("descriptive_name", identity.name);
    identity.noun = rReader.StringOr("noun", identity.name);
    identity.adjective = rReader.StringOr("adjective", identity.name);
    identity.participatesInCouncil = rReader.BoolOr("participates_in_council", true);
    if (rReader.Has("species"))
    {
        identity.species = ParseFactionSpecies_(rReader);
    }
    return identity;
}

LeaderConfig ParseLeader_(const ConfigReader& rReader)
{
    LeaderConfig leader;
    leader.name = rReader.String("name");
    leader.title = rReader.StringOr("title", "");
    return leader;
}

AITendenciesConfig ParseAITendencies_(const ConfigReader& rReader)
{
    AITendenciesConfig ai;
    ai.wealth = rReader.BoolOr("wealth", false);
    ai.power = rReader.BoolOr("power", false);
    ai.growth = rReader.BoolOr("growth", false);
    ai.tech = rReader.BoolOr("tech", false);
    return ai;
}

std::vector<std::string> ParseBaseNames_(const std::string& rPath)
{
    const nlohmann::json names = JsonConfigLoader::ReadFile(rPath);
    if (!names.is_array())
    {
        throw std::runtime_error(rPath + " must be a JSON array of strings");
    }
    std::vector<std::string> baseNames;
    for (const nlohmann::json& rName : names)
    {
        if (!rName.is_string())
        {
            throw std::runtime_error(rPath + " must be a JSON array of strings");
        }
        baseNames.push_back(rName.get<std::string>());
    }
    return baseNames;
}

std::vector<EffectConfig_t> ParseEffects_(const ConfigReader& rReader, const std::string& rId)
{
    return EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::Faction, rId);
}

FactionConfig_t ParseFactionDirectory_(const std::string& id, const std::string& dirPath)
{
    FactionConfig_t config;
    config.id = id;
    config.identity = ParseFactionFile_<FactionIdentityConfig>(dirPath + "/" + k_IdentityFile,
                                                               ParseIdentity_);
    config.leader = ParseFactionFile_<LeaderConfig>(dirPath + "/" + k_LeaderFile, ParseLeader_);
    config.ai = ParseFactionFile_<AITendenciesConfig>(dirPath + "/" + k_AIFile,
                                                      ParseAITendencies_);
    config.effects = ParseFactionFile_<std::vector<EffectConfig_t>>(
        dirPath + "/" + k_EffectsFile,
        [&id](const ConfigReader& rReader) { return ParseEffects_(rReader, id); });
    config.flavor.baseNames = ParseBaseNames_(dirPath + "/" + k_BaseNamesFile);
    return config;
}

// Sorted, so faction order does not depend on the filesystem.
std::vector<std::filesystem::path> SortedSubdirectories_(const std::string& rPath)
{
    std::vector<std::filesystem::path> dirs;
    for (const auto& rEntry : std::filesystem::directory_iterator(rPath))
    {
        if (rEntry.is_directory())
        {
            dirs.push_back(rEntry.path());
        }
    }
    std::sort(dirs.begin(), dirs.end());
    return dirs;
}

} // namespace

std::vector<FactionConfig_t> FactionConfigParser::ParseConfig(const std::string& configPath)
{
    namespace fs = std::filesystem;

    std::cout << "Loading faction configuration from: " << configPath << "\n";

    if (!fs::exists(configPath))
    {
        throw std::runtime_error("Faction config path does not exist: " + configPath);
    }

    if (!fs::is_directory(configPath))
    {
        throw std::runtime_error("Faction config path must be a directory: " + configPath);
    }
    std::vector<FactionConfig_t> configs;
    for (const fs::path& rDir : SortedSubdirectories_(configPath))
    {
        configs.push_back(ParseFactionDirectory_(rDir.filename().string(), rDir.string()));
    }

    std::cout << "Loaded " << configs.size() << " faction configurations\n";
    return configs;
}

} // namespace ac
