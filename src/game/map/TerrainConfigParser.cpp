#include "game/map/TerrainConfig.h"

#include "game/effects/TriggeredEffectParser.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainOperationRegistry.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

std::vector<ImprovementConfig_t> ParseFeatures_(const ConfigReader& rReader)
{
    std::vector<ImprovementConfig_t> configs;
    for (const ConfigReader& rEntry : rReader.Objects("features"))
    {
        configs.push_back(ParseImprovementBody(rEntry, OccupantPlacement_t::Terrain));
    }
    return configs;
}

TerrainOperationConfig_t ParseOperation_(const ConfigReader& rReader)
{
    TerrainOperationConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.energyCostSource =
        rReader.Has("energy_cost_source")
            ? rReader.Enum<EnergyCostSource_t>("energy_cost_source", "energy_cost_source")
            : EnergyCostSource_t::Flat;
    config.formerDomain = rReader.Has("former_domain")
                              ? rReader.Enum<FormerDomainRule_t>("former_domain", "former_domain")
                              : FormerDomainRule_t::MatchesTile;
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_complete_effects", config.id);
    if (config.onCompleteEffects.empty())
    {
        rReader.FailKey("on_complete_effects",
                        "must name at least one effect, or the project can never do anything");
    }
    config.project.turnsRequired = rReader.IntAtLeast("turns_required", 1);
    config.project.energyCost =
        rReader.Has("energy_cost") ? rReader.IntAtLeast("energy_cost", 0) : 0;
    config.project.requiredTech = rReader.StringOr("required_tech", "");
    return config;
}

std::vector<TerrainOperationConfig_t> ParseOperations_(const ConfigReader& rReader)
{
    std::vector<TerrainOperationConfig_t> configs;
    std::unordered_set<std::string> seen;
    for (const ConfigReader& rEntry : rReader.Objects("operations"))
    {
        TerrainOperationConfig_t config = ParseOperation_(rEntry);
        if (!seen.insert(config.id).second)
        {
            rReader.FailKey("operations", "repeats terrain operation '" + config.id + "'");
        }
        configs.push_back(std::move(config));
    }
    return configs;
}

} // namespace

TerrainFile_t ParseTerrainFile(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<TerrainFile_t>(
        configPath, "terrain", [](const ConfigReader& rReader) {
            TerrainFile_t file;
            file.features = ParseFeatures_(rReader);
            file.operations = ParseOperations_(rReader);
            return file;
        });
}

void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      ImprovementRegistry& rOccupants, TerrainOperationRegistry& rOperations)
{
    TerrainFile_t terrain = ParseTerrainFile(rTerrainPath);
    rOperations.Assign(std::move(terrain.operations));
    std::vector<ImprovementConfig_t> occupants =
        LoadTileOccupants(rImprovementsPath, std::move(terrain.features));
    rOccupants.Assign(std::move(occupants));

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
        configPath, "terrain operation", ParseOperations_);
}

} // namespace ac
