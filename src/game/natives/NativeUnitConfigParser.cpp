#include "game/natives/NativeUnitConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>

namespace ac
{

namespace
{

std::vector<std::string> ParseRequiresAnyTech_(const ConfigReader& rReader)
{
    std::vector<std::string> techs = rReader.StringArray("requires_any_tech");
    for (const std::string& rTech : techs)
    {
        if (rTech.empty())
        {
            rReader.FailKey("requires_any_tech", "entries must be non-empty strings");
        }
    }
    return techs;
}

std::vector<NativeFaunaRole_t> ParseFaunaRoles_(const ConfigReader& rReader)
{
    rReader.Array("fauna_roles");
    std::vector<NativeFaunaRole_t> roles;
    for (const std::string& rRole : rReader.StringArray("fauna_roles"))
    {
        if (rRole.empty())
        {
            rReader.FailKey("fauna_roles", "entries must be non-empty strings");
        }
        roles.push_back(ParseNativeFaunaRole(rRole));
    }
    return roles;
}

int ParseFaunaWeight_(const ConfigReader& rReader, const NativeUnitPlanetConfig_t& rPlanet)
{
    if (!rPlanet.faunaRoles.empty())
    {
        return rReader.IntAtLeast("fauna_weight", 1);
    }
    if (rReader.Has("fauna_weight"))
    {
        rReader.FailKey("fauna_weight", "is only valid when fauna_roles is non-empty");
    }
    return rPlanet.faunaWeight;
}

NativeUnitPlanetConfig_t ParsePlanet_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys(
        {"fauna_roles", "fauna_weight", "requires_any_tech", "lifecycle", "lifecycle_bonus"});

    NativeUnitPlanetConfig_t planet;
    planet.faunaRoles = ParseFaunaRoles_(rReader);
    planet.faunaWeight = ParseFaunaWeight_(rReader, planet);
    planet.requiresAnyTech = ParseRequiresAnyTech_(rReader);
    planet.lifecycle = ParseNativeLifecycleKind(rReader.String("lifecycle"));
    planet.lifecycleBonus = rReader.IntOr("lifecycle_bonus", planet.lifecycleBonus);
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
    return JsonConfigLoader::LoadObjectFile<std::vector<NativeUnitConfig_t>>(
        rConfigPath, "native units", [](const ConfigReader& rReader) {
            rReader.RejectUnknownKeys({"units"});
            std::vector<NativeUnitConfig_t> units;
            for (const ConfigReader& rUnit : rReader.Objects("units"))
            {
                units.push_back(ParseNativeUnitConfig(rUnit));
            }
            return units;
        });
}

NativeUnitConfig_t NativeUnitConfigParser::ParseNativeUnitConfig(const ConfigReader& rReader)
{
    NativeUnitConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.domain = EffectConfigParser::ParseUnitDomain(rReader.String("domain"));
    config.plan = ParseUnitPlan(rReader.String("plan"));
    config.mineralCost = rReader.IntOr("mineral_cost", 0);
    config.effects =
        EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::NativeUnit, config.id);
    config.onHoldEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_hold_effects", config.id);
    if (rReader.Has("planet") && !rReader.Json().at("planet").is_null())
    {
        config.planet = ParsePlanet_(rReader.Object("planet"));
    }
    return config;
}

} // namespace ac
