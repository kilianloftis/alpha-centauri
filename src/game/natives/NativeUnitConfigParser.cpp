#include "game/natives/NativeUnitConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/ConfigFields.h"

#include <fstream>
#include <stdexcept>

namespace ac
{

namespace
{

NativeFaunaRole_t ParseRole_(const nlohmann::json& rEntry, const std::string& rUnitId)
{
    if (!rEntry.is_string() || rEntry.get<std::string>().empty())
    {
        throw std::runtime_error("Native unit '" + rUnitId
                                 + "' 'fauna_roles' entries must be non-empty strings");
    }
    return ParseNativeFaunaRole(rEntry.get<std::string>());
}

NativeUnitPlanetConfig_t ParsePlanet_(const nlohmann::json& rJson, const std::string& rUnitId)
{
    const std::string ctx = "Native unit '" + rUnitId + "' planet";
    ConfigFields::RejectUnknownKeys(
        rJson,
        {"fauna_roles", "fauna_weight", "requires_any_tech", "lifecycle", "lifecycle_bonus"},
        ctx);

    NativeUnitPlanetConfig_t planet;
    if (!rJson.contains("fauna_roles") || !rJson.at("fauna_roles").is_array())
    {
        throw std::runtime_error(ctx + ": 'fauna_roles' must be an array");
    }
    for (const nlohmann::json& rRole : rJson.at("fauna_roles"))
    {
        planet.faunaRoles.push_back(ParseRole_(rRole, rUnitId));
    }

    if (!planet.faunaRoles.empty())
    {
        if (!rJson.contains("fauna_weight") || !rJson.at("fauna_weight").is_number_integer())
        {
            throw std::runtime_error(ctx + ": 'fauna_weight' must be an integer when roles are "
                                           "non-empty");
        }
        planet.faunaWeight = rJson.at("fauna_weight").get<int>();
        if (planet.faunaWeight <= 0)
        {
            throw std::runtime_error(ctx + ": 'fauna_weight' must be > 0");
        }
    }
    else if (rJson.contains("fauna_weight"))
    {
        throw std::runtime_error(ctx + ": 'fauna_weight' is only valid when fauna_roles is "
                                       "non-empty");
    }

    if (rJson.contains("requires_any_tech"))
    {
        const nlohmann::json& rArr = rJson.at("requires_any_tech");
        if (!rArr.is_array())
        {
            throw std::runtime_error(ctx + ": 'requires_any_tech' must be an array");
        }
        for (const nlohmann::json& rEntry : rArr)
        {
            if (!rEntry.is_string() || rEntry.get<std::string>().empty())
            {
                throw std::runtime_error(ctx + ": 'requires_any_tech' entries must be non-empty "
                                               "strings");
            }
            planet.requiresAnyTech.push_back(rEntry.get<std::string>());
        }
    }

    if (!rJson.contains("lifecycle") || !rJson.at("lifecycle").is_string())
    {
        throw std::runtime_error(ctx + ": 'lifecycle' must be a string");
    }
    planet.lifecycle = ParseNativeLifecycleKind(rJson.at("lifecycle").get<std::string>());

    if (rJson.contains("lifecycle_bonus"))
    {
        if (!rJson.at("lifecycle_bonus").is_number_integer())
        {
            throw std::runtime_error(ctx + ": 'lifecycle_bonus' must be an integer");
        }
        planet.lifecycleBonus = rJson.at("lifecycle_bonus").get<int>();
    }
    return planet;
}

} // namespace

NativeFaunaRole_t ParseNativeFaunaRole(const std::string& rRole)
{
    if (rRole == "boat_hull")
    {
        return NativeFaunaRole_t::BoatHull;
    }
    if (rRole == "boat_cargo")
    {
        return NativeFaunaRole_t::BoatCargo;
    }
    if (rRole == "boat_escort")
    {
        return NativeFaunaRole_t::BoatEscort;
    }
    if (rRole == "ambush")
    {
        return NativeFaunaRole_t::Ambush;
    }
    if (rRole == "swarm")
    {
        return NativeFaunaRole_t::Swarm;
    }
    if (rRole == "swarm_lead")
    {
        return NativeFaunaRole_t::SwarmLead;
    }
    throw std::runtime_error("Unknown native fauna role '" + rRole + "'");
}

NativeLifecycleKind_t ParseNativeLifecycleKind(const std::string& rKind)
{
    if (rKind == "none")
    {
        return NativeLifecycleKind_t::None;
    }
    if (rKind == "turn_floor")
    {
        return NativeLifecycleKind_t::TurnFloor;
    }
    if (rKind == "adjacent_fungus")
    {
        return NativeLifecycleKind_t::AdjacentFungus;
    }
    throw std::runtime_error("Unknown native lifecycle kind '" + rKind + "'");
}

std::vector<NativeUnitConfig_t> NativeUnitConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::ifstream file(rConfigPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open native units '" + rConfigPath + "'");
    }
    const nlohmann::json json = nlohmann::json::parse(file);
    if (!json.is_object())
    {
        throw std::runtime_error("native_units root must be an object");
    }
    ConfigFields::RejectUnknownKeys(json, {"units"}, "native_units");

    if (!json.contains("units") || !json.at("units").is_array())
    {
        throw std::runtime_error("native_units 'units' must be an array");
    }

    std::vector<NativeUnitConfig_t> units;
    for (const nlohmann::json& rUnit : json.at("units"))
    {
        units.push_back(ParseNativeUnitConfig(rUnit));
    }
    return units;
}

NativeUnitConfig_t NativeUnitConfigParser::ParseNativeUnitConfig(const nlohmann::json& rJson)
{
    NativeUnitConfig_t config;
    config.id = ConfigFields::ParseId(rJson);
    config.name = ConfigFields::ParseName(rJson, config.id, "name");
    if (!rJson.contains("domain"))
    {
        throw std::runtime_error("Native unit '" + config.id + "' missing required 'domain'");
    }
    config.domain = EffectConfigParser::ParseUnitDomain(rJson.at("domain").get<std::string>());
    if (!rJson.contains("plan"))
    {
        throw std::runtime_error("Native unit '" + config.id + "' missing required 'plan'");
    }
    config.plan = ParseUnitPlan(rJson.at("plan").get<std::string>());
    config.mineralCost = rJson.value("mineral_cost", 0);
    config.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::NativeUnit, config.id);
    config.onHoldEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rJson, "on_hold_effects", config.id);
    if (rJson.contains("planet"))
    {
        if (rJson.at("planet").is_null())
        {
            return config;
        }
        if (!rJson.at("planet").is_object())
        {
            throw std::runtime_error("Native unit '" + config.id + "' 'planet' must be an object");
        }
        config.planet = ParsePlanet_(rJson.at("planet"), config.id);
    }
    return config;
}

} // namespace ac
