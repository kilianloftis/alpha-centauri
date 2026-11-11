#include "game/map/ImprovementConfigParser.h"
#include "game/display/OccupantArtParser.h"
#include "game/map/TerrainConfig.h"
#include "game/map/Tile.h"
#include "game/movement/MovementConstants.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/Rational.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/effects/EffectEnums.h"

#include <algorithm>
#include <iterator>
#include <magic_enum.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

// Implicit tag every former-buildable improvement carries. Lets one `excludes` entry stand
// for "anything a former builds" without each improvement restating the relationship.
constexpr std::string_view k_BuildableTag = "buildable";

// Ensures the rational is non-negative and lands on a whole number of move fragments.
// Done at parse so hot-path move-cost code never converts rationals.
int ParseMoveCostFragments_(const ConfigReader& rReader, const char* pKey)
{
    const Rational_t cost = rReader.Rational(pKey);
    if (cost.numerator < 0)
    {
        rReader.FailKey(pKey, "must be non-negative");
    }
    try
    {
        return cost.ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint);
    }
    catch (const std::exception& rError)
    {
        rReader.FailKey(pKey, rError.what());
    }
}

using TagIndex_t = std::unordered_map<std::string, std::vector<std::string>>;

// Derived, not authored, so a rule about "anything a former builds" covers future
// improvements. Declaring it by hand is rejected.
TagIndex_t BuildTagIndex_(const std::vector<ImprovementConfig_t>& rConfigs)
{
    TagIndex_t tagToIds;
    for (const ImprovementConfig_t& rConfig : rConfigs)
    {
        if (IsBuildable(rConfig))
        {
            tagToIds[std::string(k_BuildableTag)].push_back(rConfig.id);
        }
        for (const std::string& rTag : rConfig.tags)
        {
            if (rTag.empty())
            {
                throw std::runtime_error(
                    "Improvement '" + rConfig.id + "': tags entries must be non-empty");
            }
            if (rTag == k_BuildableTag)
            {
                throw std::runtime_error("Improvement '" + rConfig.id + "': '"
                                         + std::string(k_BuildableTag)
                                         + "' is a reserved tag, set by declaring "
                                           "'turns_required' rather than by hand");
            }
            tagToIds[rTag].push_back(rConfig.id);
        }
    }
    return tagToIds;
}

// The ids an `@tag` entry names.
const std::vector<std::string>& TaggedIds_(const TagIndex_t& rTagIndex, const std::string& rEntry,
                                           std::string_view field, std::string_view id)
{
    const std::string tag = rEntry.substr(1);
    if (tag.empty())
    {
        throw std::runtime_error(
            "Improvement '" + std::string(id) + "' field '" + std::string(field)
            + "': tag reference '@' is missing a tag name");
    }
    const auto it = rTagIndex.find(tag);
    if (it == rTagIndex.end())
    {
        throw std::runtime_error(
            "Improvement '" + std::string(id) + "' field '" + std::string(field)
            + "': unknown tag '@" + tag + "'");
    }
    return it->second;
}

// Replaces each `@tag` with the ids carrying it, dropping duplicates.
std::vector<std::string> ExpandTagRefs_(const TagIndex_t& rTagIndex,
                                        const std::vector<std::string>& rRaw,
                                        std::string_view field, std::string_view id)
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
        for (const std::string& rTaggedId : TaggedIds_(rTagIndex, rEntry, field, id))
        {
            // Skip self: a feature tagged X that lists @X must not suppress/exclude
            // its own yields (ThermalBorehole + @land_terraform).
            if (rTaggedId != id && seen.insert(rTaggedId).second)
            {
                expanded.push_back(rTaggedId);
            }
        }
    }
    return expanded;
}

void ExpandConfigTagRefs_(const TagIndex_t& rTagIndex, ImprovementConfig_t& rConfig)
{
    rConfig.excludes = ExpandTagRefs_(rTagIndex, rConfig.excludes, "excludes", rConfig.id);
    rConfig.suppressYieldSources = ExpandTagRefs_(rTagIndex, rConfig.suppressYieldSources,
                                                  "suppress_yield_sources", rConfig.id);
    rConfig.suppressTerrain =
        ExpandTagRefs_(rTagIndex, rConfig.suppressTerrain, "suppress_terrain", rConfig.id);
    if (rConfig.art)
    {
        rConfig.art->hides = ExpandTagRefs_(rTagIndex, rConfig.art->hides, "art.hides", rConfig.id);
    }
}

void RequireKnownIds_(const std::unordered_set<std::string>& rIds,
                      const ImprovementConfig_t& rConfig, std::string_view field,
                      const std::vector<std::string>& rReferenced)
{
    for (const std::string& rId : rReferenced)
    {
        if (!rId.empty() && !rIds.contains(rId))
        {
            throw std::runtime_error("Improvement '" + rConfig.id + "' field '"
                                     + std::string(field) + "': unknown id '" + rId + "'");
        }
    }
}

void RequireKnownReferences_(const std::vector<ImprovementConfig_t>& rConfigs)
{
    std::unordered_set<std::string> ids;
    for (const ImprovementConfig_t& rConfig : rConfigs)
    {
        ids.insert(rConfig.id);
    }
    for (const ImprovementConfig_t& rConfig : rConfigs)
    {
        RequireKnownIds_(ids, rConfig, "suppress_terrain", rConfig.suppressTerrain);
        if (!rConfig.art)
        {
            continue;
        }
        RequireKnownIds_(ids, rConfig, "art.hides", rConfig.art->hides);
        if (const auto* pTiles = std::get_if<OccupantTileSet_t>(&rConfig.art->sprites))
        {
            RequireKnownIds_(ids, rConfig, "art.tiles.link_occupants", pTiles->linkOccupants);
            RequireKnownIds_(ids, rConfig, "art.tiles.replaces_links_of",
                             {pTiles->replacesLinksOf});
        }
    }
}

void RejectProjectFields_(const ConfigReader& rReader)
{
    for (const char* pField : {"turns_required", "energy_cost", "required_tech"})
    {
        if (rReader.Has(pField))
        {
            rReader.FailKey(pField, "belongs on a terrain operation, not on a terrain feature");
        }
    }
}

// Absent when turns_required is 0: nothing a former can build.
std::optional<FormerProject_t> ParseFormerProject_(const ConfigReader& rReader)
{
    FormerProject_t project;
    project.turnsRequired =
        rReader.Has("turns_required") ? rReader.IntAtLeast("turns_required", 0) : 0;
    project.energyCost = rReader.Has("energy_cost") ? rReader.IntAtLeast("energy_cost", 0) : 0;
    project.requiredTech = rReader.StringOr("required_tech", "");
    if (project.turnsRequired == 0 && (project.energyCost != 0 || !project.requiredTech.empty()))
    {
        rReader.Fail("'energy_cost' / 'required_tech' need a 'turns_required' > 0, or no former "
                     "can ever build it");
    }
    if (project.turnsRequired == 0)
    {
        return std::nullopt;
    }
    return project;
}

std::optional<ImprovementDomain_t> ParseImprovementDomain_(const ConfigReader& rReader)
{
    if (!rReader.Has("domain") || rReader.Json().at("domain").is_null())
    {
        return std::nullopt;
    }
    return rReader.Enum<ImprovementDomain_t>("domain", "improvement domain");
}

} // namespace

bool IsBuildable(const ImprovementConfig_t& rConfig)
{
    return rConfig.project.has_value();
}

std::vector<ImprovementConfig_t> ParseImprovementsUnexpanded(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadPath<ImprovementConfig_t>(
        rConfigPath, "improvement", [](const ConfigReader& rReader) {
            return ParseImprovementBody(rReader, OccupantPlacement_t::Improvement);
        });
}

std::vector<ImprovementConfig_t> ImprovementConfigParser::ParseConfig(const std::string& configPath)
{
    std::vector<ImprovementConfig_t> configs = ParseImprovementsUnexpanded(configPath);
    ExpandFeatureTagReferences(configs);
    return configs;
}

std::vector<ImprovementConfig_t> LoadTileOccupants(
    const std::string& rImprovementsPath, std::vector<ImprovementConfig_t> terrainFeatures)
{
    std::vector<ImprovementConfig_t> occupants =
        ParseImprovementsUnexpanded(rImprovementsPath);
    occupants.insert(occupants.end(), std::make_move_iterator(terrainFeatures.begin()),
                     std::make_move_iterator(terrainFeatures.end()));
    ExpandFeatureTagReferences(occupants);
    return occupants;
}

void ExpandFeatureTagReferences(std::vector<ImprovementConfig_t>& rConfigs)
{
    const TagIndex_t tagIndex = BuildTagIndex_(rConfigs);
    for (ImprovementConfig_t& rConfig : rConfigs)
    {
        ExpandConfigTagRefs_(tagIndex, rConfig);
    }
    RequireKnownReferences_(rConfigs);
}

// Sight radius from ThisTile Vision StatModifiers — same amount encoding as unit Vision
// (Sensor declares amount: 2).
int ResolveVisionRadius_(const ImprovementConfig_t& rConfig)
{
    int sight = 0;
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        if (rEffect.scope != EffectScope_t::ThisTile)
        {
            continue;
        }
        const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pMod || pMod->stat != StatId_t::Vision)
        {
            continue;
        }

        const ActiveEffect_t active(rEffect, rConfig.id, nullptr);
        const std::vector<ActiveEffect_t> visionEffects{active};
        const double vision =
            ResolveStatModifiers(visionEffects, SeedFor(StatId_t::Vision), nullptr).total;
        const int range = FinalizeResolvedStat(vision);
        sight = std::max(sight, range);
    }
    return sight;
}

ImprovementConfig_t ParseImprovementBody(const ConfigReader& rReader,
                                         OccupantPlacement_t placement)
{
    ImprovementConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.description = rReader.StringOr("description", "");
    config.placement = placement;
    if (placement == OccupantPlacement_t::Terrain)
    {
        RejectProjectFields_(rReader);
    }
    else
    {
        config.project = ParseFormerProject_(rReader);
    }
    config.ownedByTerritory = rReader.BoolOr("owned_by_territory", false);
    config.frequency = rReader.IntOr("frequency", 0);
    config.art = ParseOccupantArt(rReader);
    config.tags = rReader.StringArray("tags");
    config.domain = ParseImprovementDomain_(rReader);
    config.excludes = rReader.StringArray("excludes");
    config.suppressYieldSources = rReader.StringArray("suppress_yield_sources");
    config.suppressTerrain = rReader.StringArray("suppress_terrain");
    config.terminatesRiver = rReader.BoolOr("terminates_river", false);
    if (rReader.Has("move_cost"))
    {
        config.moveCostFragments = ParseMoveCostFragments_(rReader, "move_cost");
    }
    config.effects = EffectConfigParser::ParseEffects(rReader.Json(),
                                                      EffectSourceKind_t::Improvement, config.id);
    config.onVisitEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_visit_effects", config.id);
    config.visionRadius = ResolveVisionRadius_(config);
    return config;
}

} // namespace ac
