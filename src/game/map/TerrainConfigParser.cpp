#include "game/map/TerrainConfig.h"

#include "game/effects/TriggeredEffectParser.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainOperationRegistry.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <magic_enum.hpp>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

const nlohmann::json& RequireArray_(const nlohmann::json& rRoot, const char* pKey,
                                    const std::string& rPath)
{
    if (!rRoot.contains(pKey) || !rRoot.at(pKey).is_array())
    {
        throw std::runtime_error("Terrain config '" + rPath + "': expected a '" + pKey
                                 + "' array");
    }
    return rRoot.at(pKey);
}

std::vector<ImprovementConfig_t> ParseFeatures_(const nlohmann::json& rRoot,
                                                const std::string& rPath)
{
    const nlohmann::json& rFeatures = RequireArray_(rRoot, "features", rPath);
    std::vector<ImprovementConfig_t> configs;
    configs.reserve(rFeatures.size());
    for (const nlohmann::json& rEntry : rFeatures)
    {
        configs.push_back(ParseImprovementBody(rEntry, OccupantPlacement_t::Terrain));
    }
    return configs;
}

template <typename TEnum>
TEnum ParseEnumField_(const nlohmann::json& rJson, const char* pField, TEnum fallback,
                      const std::string& rId)
{
    if (!rJson.contains(pField))
    {
        return fallback;
    }
    if (!rJson.at(pField).is_string())
    {
        throw std::runtime_error("Terrain operation '" + rId + "': '" + pField
                                 + "' must be a string");
    }
    const std::string value = rJson.at(pField).get<std::string>();
    const auto parsed = magic_enum::enum_cast<TEnum>(value, magic_enum::case_insensitive);
    if (!parsed)
    {
        throw std::runtime_error("Terrain operation '" + rId + "': unknown " + pField + " '"
                                 + value + "'");
    }
    return *parsed;
}

TerrainOperationConfig_t ParseOperation_(const nlohmann::json& rJson)
{
    TerrainOperationConfig_t config;
    config.id = ConfigFields::ParseId(rJson);
    config.name = ConfigFields::ParseName(rJson, config.id);
    config.energyCostSource = ParseEnumField_(rJson, "energy_cost_source",
                                              EnergyCostSource_t::Flat, config.id);
    config.formerDomain =
        ParseEnumField_(rJson, "former_domain", FormerDomainRule_t::MatchesTile, config.id);
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rJson, "on_complete_effects", config.id);
    if (config.onCompleteEffects.empty())
    {
        throw std::runtime_error("Terrain operation '" + config.id
                                 + "': 'on_complete_effects' must name at least one effect, or "
                                   "the project can never do anything");
    }
    config.project.turnsRequired = rJson.value("turns_required", 0);
    if (config.project.turnsRequired <= 0)
    {
        throw std::runtime_error("Terrain operation '" + config.id
                                 + "': 'turns_required' must be > 0");
    }
    config.project.energyCost = rJson.value("energy_cost", 0);
    if (config.project.energyCost < 0)
    {
        throw std::runtime_error("Terrain operation '" + config.id
                                 + "': 'energy_cost' must be >= 0");
    }
    config.project.requiredTech = ConfigFields::ParseRequiredTech(rJson);
    return config;
}

std::vector<TerrainOperationConfig_t> ParseOperations_(const nlohmann::json& rRoot,
                                                       const std::string& rPath)
{
    const nlohmann::json& rOperations = RequireArray_(rRoot, "operations", rPath);
    std::vector<TerrainOperationConfig_t> configs;
    std::unordered_set<std::string> seen;
    for (const nlohmann::json& rEntry : rOperations)
    {
        TerrainOperationConfig_t config = ParseOperation_(rEntry);
        if (!seen.insert(config.id).second)
        {
            throw std::runtime_error("Duplicate terrain operation '" + config.id + "'");
        }
        configs.push_back(std::move(config));
    }
    return configs;
}

} // namespace

TerrainFile_t ParseTerrainFile(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<TerrainFile_t>(
        configPath, "terrain",
        [&configPath](const nlohmann::json& rRoot)
        {
            TerrainFile_t file;
            file.features = ParseFeatures_(rRoot, configPath);
            file.operations = ParseOperations_(rRoot, configPath);
            return file;
        });
}

void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      ImprovementRegistry& rOccupants, TerrainOperationRegistry& rOperations)
{
    TerrainFile_t terrain = ParseTerrainFile(rTerrainPath);
    rOperations.Assign(std::move(terrain.operations));
    rOccupants.Assign(LoadTileOccupants(rImprovementsPath, std::move(terrain.features)));

    for (const TerrainOperationConfig_t& rOperation : rOperations.GetAll())
    {
        const ImprovementConfig_t* pClash = rOccupants.Find(rOperation.id);
        if (pClash && IsBuildable(*pClash))
        {
            throw std::runtime_error("Terrain operation '" + rOperation.id
                                     + "' is shadowed by a buildable improvement of the same id");
        }
    }
}

std::vector<TerrainOperationConfig_t> TerrainOperationConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<std::vector<TerrainOperationConfig_t>>(
        configPath, "terrain operation",
        [&configPath](const nlohmann::json& rRoot) { return ParseOperations_(rRoot, configPath); });
}

} // namespace ac
