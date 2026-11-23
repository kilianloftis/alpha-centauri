#include "game/map/TerrainConfig.h"

#include "game/effects/TriggeredEffectParser.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/research/RequiredTechsParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerraformProjectRegistry.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <unordered_set>
#include <utility>

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

std::vector<std::string> ExpandBlockedBy_(
    const std::unordered_map<std::string, std::vector<std::string>>& rTagIndex,
    const std::vector<std::string>& rRaw, std::string_view projectId)
{
    std::vector<std::string> expanded;
    std::unordered_set<std::string> seen;
    for (const std::string& rEntry : rRaw)
    {
        if (rEntry.empty() || rEntry.front() != '@')
        {
            if (seen.insert(rEntry).second)
            {
                expanded.push_back(rEntry);
            }
            continue;
        }
        const std::string tag = rEntry.substr(1);
        if (tag.empty())
        {
            throw std::runtime_error("Project '" + std::string(projectId)
                                     + "' blocked_by: tag reference '@' is missing a tag name");
        }
        const auto it = rTagIndex.find(tag);
        if (it == rTagIndex.end())
        {
            throw std::runtime_error("Project '" + std::string(projectId)
                                     + "' blocked_by: unknown tag '@" + tag + "'");
        }
        for (const std::string& rTaggedId : it->second)
        {
            if (seen.insert(rTaggedId).second)
            {
                expanded.push_back(rTaggedId);
            }
        }
    }
    return expanded;
}

std::unordered_map<std::string, std::vector<std::string>> OccupantTagIndex_(
    const std::vector<ImprovementConfig_t>& rOccupants)
{
    std::unordered_map<std::string, std::vector<std::string>> tagToIds;
    for (const ImprovementConfig_t& rConfig : rOccupants)
    {
        if (IsBuildable(rConfig))
        {
            tagToIds["buildable"].push_back(rConfig.id);
        }
        for (const std::string& rTag : rConfig.tags)
        {
            tagToIds[rTag].push_back(rConfig.id);
        }
    }
    return tagToIds;
}

TerraformProjectConfig_t ParseProject_(const ConfigReader& rReader)
{
    TerraformProjectConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.formerDomain = rReader.Has("former_domain")
                              ? rReader.Enum<FormerDomainRule_t>("former_domain", "former_domain")
                              : FormerDomainRule_t::MatchesTile;
    if (rReader.Has("domain") && !rReader.Json().at("domain").is_null())
    {
        config.domain = rReader.Enum<ImprovementDomain_t>("domain", "terraform project domain");
    }
    if (rReader.Has("places") && !rReader.Json().at("places").is_null())
    {
        config.places = rReader.NonEmptyString("places");
    }
    config.blockedBy = rReader.StringArray("blocked_by");
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_complete_effects", config.id);
    if (!config.places && config.onCompleteEffects.empty())
    {
        rReader.Fail("must declare 'places' and/or a non-empty 'on_complete_effects'");
    }
    config.project.turnsRequired = rReader.IntAtLeast("turns_required", 1);
    config.project.energyCost = ParseFormerEnergyCost(rReader);
    config.project.requiredTechs = ParseRequiredTechs(rReader);
    return config;
}

std::unordered_set<std::string> PlaceTargetIds_(
    const std::vector<TerraformProjectConfig_t>& rProjects)
{
    std::unordered_set<std::string> ids;
    for (const TerraformProjectConfig_t& rProject : rProjects)
    {
        if (rProject.places)
        {
            ids.insert(*rProject.places);
        }
    }
    return ids;
}

void ValidateProjects_(std::vector<TerraformProjectConfig_t>& rProjects,
                       const ImprovementRegistry& rOccupants)
{
    const std::vector<ImprovementConfig_t>& rAll = rOccupants.GetAll();
    const auto tagIndex = OccupantTagIndex_(rAll);
    std::unordered_set<std::string> occupantIds;
    for (const ImprovementConfig_t& rConfig : rAll)
    {
        occupantIds.insert(rConfig.id);
    }

    for (TerraformProjectConfig_t& rProject : rProjects)
    {
        if (rProject.places)
        {
            if (!occupantIds.contains(*rProject.places))
            {
                throw std::runtime_error("Terraform project '" + rProject.id
                                         + "' places unknown occupant '" + *rProject.places
                                         + "'");
            }
        }
        rProject.blockedBy = ExpandBlockedBy_(tagIndex, rProject.blockedBy, rProject.id);
        for (const std::string& rId : rProject.blockedBy)
        {
            if (!occupantIds.contains(rId))
            {
                throw std::runtime_error("Terraform project '" + rProject.id
                                         + "' blocked_by unknown id '" + rId + "'");
            }
        }
    }
}

} // namespace

TerrainFile_t ParseTerrainFile(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<TerrainFile_t>(
        configPath, "terrain", [](const ConfigReader& rReader) {
            TerrainFile_t file;
            file.features = ParseFeatures_(rReader);
            return file;
        });
}

void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      const std::string& rTerraformProjectsPath, ImprovementRegistry& rOccupants,
                      TerraformProjectRegistry& rProjects)
{
    TerrainFile_t terrain = ParseTerrainFile(rTerrainPath);
    std::vector<TerraformProjectConfig_t> projects =
        TerraformProjectConfigParser::ParseConfig(rTerraformProjectsPath);
    const std::unordered_set<std::string> placeTargets = PlaceTargetIds_(projects);
    std::vector<ImprovementConfig_t> occupants =
        LoadTileOccupants(rImprovementsPath, std::move(terrain.features), placeTargets);
    for (ImprovementConfig_t& rConfig : occupants)
    {
        rConfig.formerPlaceTarget = placeTargets.contains(rConfig.id);
    }
    rOccupants.Assign(std::move(occupants));
    ValidateProjects_(projects, rOccupants);
    rProjects.Assign(std::move(projects));
}

std::vector<TerraformProjectConfig_t> TerraformProjectConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<TerraformProjectConfig_t>(
        configPath, "terraform project",
        [](const ConfigReader& rReader) { return ParseProject_(rReader); });
}

} // namespace ac
