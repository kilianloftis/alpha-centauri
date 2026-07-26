#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/units/MovementConstants.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/Rational.h"
#include "game/effects/BonusEffectParser.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ac
{

namespace
{

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

TerraformResult_t ParseTerraformResult_(const nlohmann::json& improvementJson, std::string_view id)
{
    if (!improvementJson.contains("terraform"))
    {
        return TerraformResult_t::Place;
    }

    const nlohmann::json& terraform = improvementJson.at("terraform");
    if (!terraform.is_object())
    {
        throw std::runtime_error(
            "Improvement '" + std::string(id) + "': 'terraform' must be an object");
    }

    const std::string result = terraform.value("result", "place");
    if (result == "place")
    {
        return TerraformResult_t::Place;
    }
    if (result == "level_terrain")
    {
        return TerraformResult_t::LevelTerrain;
    }
    if (result == "raise_land")
    {
        return TerraformResult_t::RaiseLand;
    }
    if (result == "lower_land")
    {
        return TerraformResult_t::LowerLand;
    }
    if (result == "plant_fungus")
    {
        return TerraformResult_t::PlantFungus;
    }
    if (result == "remove_fungus")
    {
        return TerraformResult_t::RemoveFungus;
    }
    if (result == "aquifer")
    {
        return TerraformResult_t::Aquifer;
    }

    throw std::runtime_error(
        "Improvement '" + std::string(id) + "': unknown terraform.result '" + result + "'");
}

} // namespace

bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate)
{
    for (const std::string& excludedId : rCandidate.excludes)
    {
        if (rTile.HasFeature(excludedId))
            return false;
    }
    return true;
}

std::vector<ImprovementConfig_t> ImprovementConfigParser::ParseConfig(const std::string& configPath)
{
    auto configs = JsonConfigLoader::LoadPath<ImprovementConfig_t>(
        configPath, "improvement",
        [this](const nlohmann::json& rJson) { return ParseImprovementConfig(rJson); });
    ExpandTagReferences(configs);
    return configs;
}

void ImprovementConfigParser::ExpandTagReferences(std::vector<ImprovementConfig_t>& rConfigs) const
{
    std::unordered_map<std::string, std::vector<std::string>> tagToIds;
    for (const ImprovementConfig_t& rConfig : rConfigs)
    {
        for (const std::string& rTag : rConfig.tags)
        {
            if (rTag.empty())
            {
                throw std::runtime_error(
                    "Improvement '" + rConfig.id + "': tags entries must be non-empty");
            }
            tagToIds[rTag].push_back(rConfig.id);
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

    for (ImprovementConfig_t& rConfig : rConfigs)
    {
        rConfig.excludes = expand(rConfig.excludes, "excludes", rConfig.id);
        rConfig.suppressYieldSources =
            expand(rConfig.suppressYieldSources, "suppress_yield_sources", rConfig.id);
    }
}

ImprovementConfig_t ImprovementConfigParser::ParseImprovementConfig(const nlohmann::json& improvementJson)
{
    ImprovementConfig_t config;
    config.id = ConfigFields::ParseId(improvementJson);
    config.name = ConfigFields::ParseName(improvementJson, config.id);
    config.description = improvementJson.value("description", "");
    if (improvementJson.contains("mineral_cost"))
    {
        throw std::runtime_error(
            "Improvement '" + config.id
            + "': 'mineral_cost' is no longer supported; use 'turns_required' and 'energy_cost'");
    }
    config.turnsRequired = improvementJson.value("turns_required", 0);
    config.energyCost = improvementJson.value("energy_cost", 0);
    config.requiredTech = ConfigFields::ParseRequiredTech(improvementJson);
    config.ownedByTerritory = improvementJson.value("owned_by_territory", false);
    config.frequency = improvementJson.value("frequency", 0);
    config.spritePath = improvementJson.value("sprite_path", "");
    config.tags = ConfigFields::ParseStringArray(improvementJson, "tags");
    config.excludes = ConfigFields::ParseStringArray(improvementJson, "excludes");
    config.suppressYieldSources =
        ConfigFields::ParseStringArray(improvementJson, "suppress_yield_sources");
    config.terminatesRiver = improvementJson.value("terminates_river", false);
    config.terraformResult = ParseTerraformResult_(improvementJson, config.id);
    if (improvementJson.contains("move_cost"))
    {
        const Rational_t cost = Rational_t::ParseJson(improvementJson.at("move_cost"));
        config.moveCostFragments = ParseMoveCostFragments_(cost, "move_cost", config.id);
    }
    if (improvementJson.contains("move_cost_override"))
    {
        const Rational_t cost = Rational_t::ParseJson(improvementJson.at("move_cost_override"));
        config.moveCostOverrideFragments =
            ParseMoveCostFragments_(cost, "move_cost_override", config.id);
    }
    config.effects = BonusEffectParser::ParseEffects(improvementJson, EffectSourceKind_t::Improvement, config.id);

    return config;
}

} // namespace ac
