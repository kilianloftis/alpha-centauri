#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"
#include "game/units/MovementConstants.h"
#include "lib/config/ConfigFields.h"
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
int ParseMoveCostFragments_(const Rational_t& rCost, std::string_view field, std::string_view id)
{
    auto wrap = [&](const std::string& message) {
        throw std::runtime_error(
            "Improvement '" + std::string(id) + "' field '" + std::string(field) + "': " + message);
    };

    if (rCost.numerator < 0 || rCost.denominator <= 0)
    {
        wrap("move cost must be non-negative");
    }

    try
    {
        return rCost.ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint);
    }
    catch (const std::exception& e)
    {
        wrap(e.what());
        return 0; // unreachable
    }
}

} // namespace

bool IsBuildable(const ImprovementConfig_t& rConfig)
{
    return rConfig.project.has_value();
}

std::vector<ImprovementConfig_t> ParseImprovementsUnexpanded(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadPath<ImprovementConfig_t>(
        rConfigPath, "improvement",
        [](const nlohmann::json& rJson) {
            return ParseImprovementBody(rJson, OccupantPlacement_t::Improvement);
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

std::vector<ImprovementConfig_t> LoadTileOccupants(const std::string& rImprovementsPath,
                                                   const std::string& rTerrainPath)
{
    return LoadTileOccupants(rImprovementsPath, ParseTerrainFile(rTerrainPath).features);
}

void ExpandFeatureTagReferences(std::vector<ImprovementConfig_t>& rConfigs)
{
    std::vector<ImprovementConfig_t*> configs;
    configs.reserve(rConfigs.size());
    for (ImprovementConfig_t& rConfig : rConfigs)
    {
        configs.push_back(&rConfig);
    }

    std::unordered_map<std::string, std::vector<std::string>> tagToIds;
    for (const ImprovementConfig_t* pConfig : configs)
    {
        // Derived from the entry rather than authored, so a rule about "anything a former
        // builds" stays true for improvements that do not exist yet. Reserved: declaring it
        // by hand would mean two sources for one tag.
        if (IsBuildable(*pConfig))
        {
            tagToIds[std::string(k_BuildableTag)].push_back(pConfig->id);
        }
        for (const std::string& rTag : pConfig->tags)
        {
            if (rTag.empty())
            {
                throw std::runtime_error(
                    "Improvement '" + pConfig->id + "': tags entries must be non-empty");
            }
            if (rTag == k_BuildableTag)
            {
                throw std::runtime_error("Improvement '" + pConfig->id + "': '"
                                         + std::string(k_BuildableTag)
                                         + "' is a reserved tag, set by declaring "
                                           "'turns_required' rather than by hand");
            }
            tagToIds[rTag].push_back(pConfig->id);
        }
    }

    auto expand = [&](const std::vector<std::string>& rRaw, std::string_view field,
                      std::string_view id) {
        std::vector<std::string> expanded;
        std::unordered_set<std::string> seen;
        for (const std::string& rEntry : rRaw)
        {
            if (!rEntry.empty() && rEntry.front() == '@')
            {
                const std::string tag = rEntry.substr(1);
                if (tag.empty())
                {
                    throw std::runtime_error(
                        "Improvement '" + std::string(id) + "' field '" + std::string(field)
                        + "': tag reference '@' is missing a tag name");
                }
                const auto it = tagToIds.find(tag);
                if (it == tagToIds.end())
                {
                    throw std::runtime_error(
                        "Improvement '" + std::string(id) + "' field '" + std::string(field)
                        + "': unknown tag '@" + tag + "'");
                }
                for (const std::string& rTaggedId : it->second)
                {
                    // Skip self: a feature tagged X that lists @X must not suppress/exclude
                    // its own yields (ThermalBorehole + @land_terraform).
                    if (rTaggedId == id)
                    {
                        continue;
                    }
                    if (seen.insert(rTaggedId).second)
                    {
                        expanded.push_back(rTaggedId);
                    }
                }
            }
            else if (seen.insert(rEntry).second)
            {
                expanded.push_back(rEntry);
            }
        }
        return expanded;
    };

    for (ImprovementConfig_t* pConfig : configs)
    {
        pConfig->excludes = expand(pConfig->excludes, "excludes", pConfig->id);
        pConfig->suppressYieldSources =
            expand(pConfig->suppressYieldSources, "suppress_yield_sources", pConfig->id);
    }
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

        const ActiveEffect_t active(rEffect, rConfig.id);
        const int range = FinalizeResolvedStat(
            ResolveStatModifiers(std::vector<ActiveEffect_t>{active}, SeedFor(StatId_t::Vision))
                .total);
        sight = std::max(sight, range);
    }
    return sight;
}

ImprovementConfig_t ParseImprovementBody(const nlohmann::json& rImprovementJson,
                                         OccupantPlacement_t placement)
{
    ImprovementConfig_t config;
    config.id = ConfigFields::ParseId(rImprovementJson);
    config.name = ConfigFields::ParseName(rImprovementJson, config.id);
    config.description = rImprovementJson.value("description", "");
    config.placement = placement;
    if (rImprovementJson.contains("mineral_cost") || rImprovementJson.contains("terraform"))
    {
        throw std::runtime_error("Improvement '" + config.id + "': unknown field; a former "
                                 "project is 'turns_required' / 'energy_cost' on the "
                                 "improvement itself");
    }
    if (placement == OccupantPlacement_t::Terrain)
    {
        for (const char* pField : {"turns_required", "energy_cost", "required_tech"})
        {
            if (rImprovementJson.contains(pField))
            {
                throw std::runtime_error(
                    "Terrain feature '" + config.id + "': '" + pField
                    + "' belongs on a terrain operation, not on a terrain feature");
            }
        }
    }
    else
    {
        FormerProject_t project;
        project.turnsRequired = rImprovementJson.value("turns_required", 0);
        if (project.turnsRequired < 0)
        {
            throw std::runtime_error("Improvement '" + config.id
                                     + "': 'turns_required' must be >= 0");
        }
        project.energyCost = rImprovementJson.value("energy_cost", 0);
        if (project.energyCost < 0)
        {
            throw std::runtime_error("Improvement '" + config.id
                                     + "': 'energy_cost' must be >= 0");
        }
        project.requiredTech = ConfigFields::ParseRequiredTech(rImprovementJson);
        if (project.turnsRequired == 0 && (project.energyCost != 0 || !project.requiredTech.empty()))
        {
            throw std::runtime_error(
                "Improvement '" + config.id
                + "': 'energy_cost' / 'required_tech' need a 'turns_required' > 0, or no former "
                  "can ever build it");
        }
        if (project.turnsRequired > 0)
        {
            config.project = std::move(project);
        }
    }
    config.ownedByTerritory = rImprovementJson.value("owned_by_territory", false);
    config.frequency = rImprovementJson.value("frequency", 0);
    config.spritePath = rImprovementJson.value("sprite_path", "");
    config.tags = ConfigFields::ParseStringArray(rImprovementJson, "tags");
    if (rImprovementJson.contains("domain") && !rImprovementJson.at("domain").is_null())
    {
        if (!rImprovementJson.at("domain").is_string())
        {
            throw std::runtime_error("Improvement '" + config.id + "': 'domain' must be a string");
        }
        const std::string domain = rImprovementJson.at("domain").get<std::string>();
        const auto parsed =
            magic_enum::enum_cast<ImprovementDomain_t>(domain, magic_enum::case_insensitive);
        if (!parsed)
        {
            throw std::runtime_error("Improvement '" + config.id + "': unknown domain '" + domain
                                     + "'");
        }
        config.domain = *parsed;
    }
    config.excludes = ConfigFields::ParseStringArray(rImprovementJson, "excludes");
    config.suppressYieldSources =
        ConfigFields::ParseStringArray(rImprovementJson, "suppress_yield_sources");
    config.terminatesRiver = rImprovementJson.value("terminates_river", false);
    if (rImprovementJson.contains("move_cost"))
    {
        const Rational_t cost = Rational_t::ParseJson(rImprovementJson.at("move_cost"));
        config.moveCostFragments = ParseMoveCostFragments_(cost, "move_cost", config.id);
    }
    if (rImprovementJson.contains("move_cost_override"))
    {
        throw std::runtime_error(
            "Improvement '" + config.id
            + "': 'move_cost_override' is a StatModifier on move_cost with op MaxClamp");
    }
    config.effects = EffectConfigParser::ParseEffects(rImprovementJson, EffectSourceKind_t::Improvement, config.id);
    config.onVisitEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rImprovementJson, "on_visit_effects", config.id);
    config.visionRadius = ResolveVisionRadius_(config);

    return config;
}

} // namespace ac
