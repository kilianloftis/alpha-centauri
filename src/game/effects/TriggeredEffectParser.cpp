#include "game/effects/TriggeredEffectParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/Rational.h"

#include <magic_enum.hpp>

#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace ac
{
namespace TriggeredEffectParser
{

namespace
{

using ParseFn_ = std::function<void(const nlohmann::json&, TriggeredEffectConfig_t&)>;

void ParseAddBuilding_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const std::string buildingId = parameters.value("building_id", "");
    if (buildingId.empty())
    {
        throw std::runtime_error("AddBuilding requires a non-empty 'building_id'");
    }
    rEffect.effect = AddBuildingEffect_t{buildingId};
}

void ParseGrantTech_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasTechId = parameters.contains("tech_id");
    const bool bHasSelection = parameters.contains("selection");
    if (bHasTechId == bHasSelection)
    {
        throw std::runtime_error(
            "GrantTech requires exactly one of 'tech_id' or 'selection' (Available)");
    }
    GrantTechEffect_t grant;
    if (bHasTechId)
    {
        if (!parameters.at("tech_id").is_string())
        {
            throw std::runtime_error("GrantTech 'tech_id' must be a string");
        }
        grant.techId = parameters.at("tech_id").get<std::string>();
        if (grant.techId->empty())
        {
            throw std::runtime_error("GrantTech 'tech_id' must be non-empty");
        }
    }
    else
    {
        if (!parameters.at("selection").is_string())
        {
            throw std::runtime_error("GrantTech 'selection' must be a string");
        }
        const std::string selection = parameters.at("selection").get<std::string>();
        if (selection != "Available")
        {
            throw std::runtime_error(
                "GrantTech 'selection' must be 'Available' (got '" + selection + "')");
        }
    }
    rEffect.effect = grant;
}

void ParseGrantUnit_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    if (!parameters.contains("component_ids") || !parameters.at("component_ids").is_array()
        || parameters.at("component_ids").empty())
    {
        throw std::runtime_error(
            "GrantUnit requires a non-empty 'component_ids' array (a granted unit is assembled "
            "from components, like base_conquest's escape_colony_pod — there is no registry of "
            "named designs to reference)");
    }
    GrantUnitEffect_t grant;
    for (const auto& rId : parameters.at("component_ids"))
    {
        if (!rId.is_string() || rId.get<std::string>().empty())
        {
            throw std::runtime_error("GrantUnit 'component_ids' entries must be non-empty strings");
        }
        grant.componentIds.push_back(rId.get<std::string>());
    }
    grant.count = static_cast<int>(EffectConfigParser::ParseNumber(parameters, "count", 1.0));
    if (grant.count < 1)
    {
        throw std::runtime_error("GrantUnit 'count' must be >= 1");
    }
    rEffect.effect = grant;
}

void ParseGrantEnergy_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    GrantEnergyEffect_t grant;
    grant.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    rEffect.effect = grant;
}

void ParseWorldParameter_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    WorldParameterEffect_t world;
    world.parameter = ParseWorldParameterId(parameters.value("parameter", ""));
    world.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    rEffect.effect = world;
}

void ParseSetInfiltration_(const nlohmann::json& /*parameters*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = SetInfiltrationEffect_t{};
}

void ParseModifyPopulation_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    ModifyPopulationEffect_t modify;
    modify.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    modify.op = EffectConfigParser::ParseModifierOp(parameters.value("op", "Add"));
    if (modify.op != ModifierOp_t::Add && modify.op != ModifierOp_t::AddPercent)
    {
        throw std::runtime_error("ModifyPopulation op must be Add or AddPercent");
    }
    modify.minSize = static_cast<int>(EffectConfigParser::ParseNumber(parameters, "min_size", 0.0));
    if (modify.minSize < 0)
    {
        throw std::runtime_error("ModifyPopulation 'min_size' must be >= 0");
    }
    rEffect.effect = modify;
}

void ParseGrantXp_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    GrantXpEffect_t grant;
    grant.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    grant.op = EffectConfigParser::ParseModifierOp(parameters.value("op", "Add"));
    if (parameters.contains("remove_host_chance"))
    {
        grant.removeHostChance = Rational_t::ParseJson(parameters.at("remove_host_chance"));
        if (grant.removeHostChance->denominator <= 0)
        {
            throw std::runtime_error("GrantXp 'remove_host_chance' denominator must be positive");
        }
    }
    rEffect.effect = grant;
}

void ParseRestoreHitPoints_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    RestoreHitPointsEffect_t restore;
    restore.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    const std::string opStr = parameters.value("op", "Add");
    const auto op =
        magic_enum::enum_cast<RestoreHitPointsOp_t>(opStr, magic_enum::case_insensitive);
    if (!op.has_value())
    {
        throw std::runtime_error(
            "RestoreHitPoints op must be Add, AddPercent, MaxClamp, MinClamp, or SetPercent");
    }
    restore.op = *op;
    rEffect.effect = restore;
}

void ParseDestroyFacility_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const auto requireBool = [&parameters](const char* key)
    {
        if (!parameters.contains(key) || !parameters.at(key).is_boolean())
        {
            throw std::runtime_error(std::string("DestroyFacility '") + key
                                     + "' must be a boolean");
        }
        return parameters.at(key).get<bool>();
    };

    DestroyFacilityEffect_t destroy;
    if (!parameters.contains("count") || !parameters.at("count").is_number_integer())
    {
        throw std::runtime_error("DestroyFacility 'count' must be an integer");
    }
    destroy.count = parameters.at("count").get<int>();
    if (destroy.count < 1)
    {
        throw std::runtime_error("DestroyFacility 'count' must be >= 1");
    }
    destroy.excludeHq = requireBool("exclude_hq");
    destroy.excludeSecretProjects = requireBool("exclude_secret_projects");
    rEffect.effect = destroy;
}

void ParseRebel_(const nlohmann::json& /*parameters*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = RebelEffect_t{};
}

void ParseDestroyUnit_(const nlohmann::json& /*parameters*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = DestroyUnitEffect_t{};
}

void ParseEarthquake_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasLevels = parameters.contains("levels");
    const bool bHasStat = parameters.contains("levels_stat");
    if (bHasLevels == bHasStat)
    {
        throw std::runtime_error(
            "Earthquake requires exactly one of 'levels' (a fixed count) or 'levels_stat' "
            "(resolved off the subject unit)");
    }

    EarthquakeEffect_t quake;
    if (bHasLevels)
    {
        quake.levels = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "levels"));
        if (quake.levels < 1)
        {
            throw std::runtime_error("Earthquake 'levels' must be >= 1");
        }
    }
    else
    {
        if (!parameters.at("levels_stat").is_string())
        {
            throw std::runtime_error("Earthquake 'levels_stat' must be a string");
        }
        const StatId_t stat = ParseStatId(parameters.at("levels_stat").get<std::string>());
        if (DomainFor(stat) != ResolveDomain_t::Unit)
        {
            throw std::runtime_error(
                "Earthquake 'levels_stat' must be a unit stat: the magnitude is resolved off "
                "the unit the trigger stamped");
        }
        quake.levelsStat = stat;
    }
    rEffect.effect = quake;
}

void ParseFungalBloom_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasTiles = parameters.contains("tiles");
    const bool bHasStat = parameters.contains("tiles_stat");
    if (bHasTiles == bHasStat)
    {
        throw std::runtime_error(
            "FungalBloom requires exactly one of 'tiles' (a fixed count) or 'tiles_stat' "
            "(resolved off the subject unit)");
    }

    FungalBloomEffect_t bloom;
    if (bHasTiles)
    {
        bloom.tiles = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "tiles"));
        if (bloom.tiles < 1)
        {
            throw std::runtime_error("FungalBloom 'tiles' must be >= 1");
        }
    }
    else
    {
        if (!parameters.at("tiles_stat").is_string())
        {
            throw std::runtime_error("FungalBloom 'tiles_stat' must be a string");
        }
        const StatId_t stat = ParseStatId(parameters.at("tiles_stat").get<std::string>());
        if (DomainFor(stat) != ResolveDomain_t::Unit)
        {
            throw std::runtime_error(
                "FungalBloom 'tiles_stat' must be a unit stat: the size is resolved off "
                "the unit the trigger stamped");
        }
        bloom.tilesStat = stat;
    }
    rEffect.effect = bloom;
}

const std::unordered_map<std::string, ParseFn_>& TypeParsers_()
{
    static const std::unordered_map<std::string, ParseFn_> k_Parsers = {
        {"AddBuilding", ParseAddBuilding_},
        {"GrantTech", ParseGrantTech_},
        {"GrantUnit", ParseGrantUnit_},
        {"GrantEnergy", ParseGrantEnergy_},
        {"WorldParameter", ParseWorldParameter_},
        {"SetInfiltration", ParseSetInfiltration_},
        {"ModifyPopulation", ParseModifyPopulation_},
        {"GrantXp", ParseGrantXp_},
        {"RestoreHitPoints", ParseRestoreHitPoints_},
        {"DestroyFacility", ParseDestroyFacility_},
        {"Rebel", ParseRebel_},
        {"DestroyUnit", ParseDestroyUnit_},
        {"Earthquake", ParseEarthquake_},
        {"FungalBloom", ParseFungalBloom_},
    };
    return k_Parsers;
}

OncePer_t ParseOncePer_(const nlohmann::json& onceJson)
{
    OncePer_t oncePer;
    // Required, not defaulted: which subject remembers the key decides whether the entry can
    // fire at all from a given trigger, and a default of "unit" would turn a missing key into
    // an entry that silently never fires from a trigger that has no unit.
    if (!onceJson.contains("scope") || !onceJson.at("scope").is_string())
    {
        throw std::runtime_error("once_per requires a 'scope' of unit, base, faction, or world");
    }
    const std::string scopeStr = onceJson.at("scope").get<std::string>();
    const auto scope = magic_enum::enum_cast<OnceScope_t>(scopeStr, magic_enum::case_insensitive);
    if (!scope.has_value())
    {
        throw std::runtime_error("once_per has unknown 'scope': '" + scopeStr
                                 + "' (expected unit, base, faction, or world)");
    }
    oncePer.scope = *scope;
    oncePer.key = onceJson.value("key", "");
    if (oncePer.key.empty())
    {
        // The key is what makes the rule span instances (every Monolith shares one); deriving
        // it from the entry would make each monolith grant its own copy.
        throw std::runtime_error("once_per requires a non-empty 'key'");
    }
    return oncePer;
}

} // namespace

bool IsTriggeredEffectType(const std::string& rTypeName)
{
    return TypeParsers_().count(rTypeName) != 0;
}

bool IsContinuousEffectType(const std::string& rTypeName)
{
    // Asks the continuous parser's own table rather than keeping a second list beside it:
    // a new continuous type would otherwise be reported here as an unknown triggered type
    // instead of "belongs in effects".
    return EffectConfigParser::IsEffectType(rTypeName);
}

TriggeredEffectConfig_t ParseTriggeredEffectConfig(const nlohmann::json& effectJson,
                                                   const std::string& rListName)
{
    TriggeredEffectConfig_t effect;
    const std::string typeStr = effectJson.at("type").get<std::string>();

    const auto& parsers = TypeParsers_();
    const auto it = parsers.find(typeStr);
    if (it == parsers.end())
    {
        if (IsContinuousEffectType(typeStr))
        {
            throw std::runtime_error(
                "'" + typeStr + "' is a continuous effect and belongs in 'effects', not '"
                + rListName + "'; a triggered list holds one-shot effects only");
        }
        throw std::runtime_error("Unknown triggered effect type: '" + typeStr + "' in '"
                                 + rListName + "'");
    }

    if (effectJson.contains("factionFilter"))
    {
        // Only SetInfiltration picks its own targets; every other type acts on the subjects
        // the context supplies. Accepting a filter elsewhere would read as narrowing the
        // targets while changing nothing — a council GrantEnergy with CouncilMembers still
        // credits whoever the applier listed.
        if (typeStr != "SetInfiltration")
        {
            throw std::runtime_error(
                "Triggered effect '" + typeStr + "' in '" + rListName
                + "' cannot carry 'factionFilter': only SetInfiltration selects its own "
                  "targets. Everything else applies to the subjects its trigger supplies");
        }
        effect.factionFilter =
            EffectConfigParser::ParseFactionFilter(effectJson.at("factionFilter"));
    }
    if (effectJson.contains("once_per"))
    {
        effect.oncePer = ParseOncePer_(effectJson.at("once_per"));
    }
    if (effectJson.contains("condition"))
    {
        effect.condition = EffectConfigParser::ParseCondition(effectJson.at("condition"));
    }
    // Trigger timing comes from the list this entry sits in, and a triggered effect resolves
    // against an explicit context rather than a scope lane — so the continuous-only keys are
    // rejected rather than silently ignored.
    for (const char* pKey : {"scope", "persistence", "radius", "min_radius",
                             "buildingFilter", "removed_by_tech"})
    {
        if (effectJson.contains(pKey))
        {
            throw std::runtime_error(std::string("Triggered effect '") + typeStr + "' in '"
                                     + rListName + "' cannot carry '" + pKey
                                     + "': it fires when its list's trigger fires, against that "
                                       "trigger's subject");
        }
    }

    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());
    it->second(parameters, effect);
    return effect;
}

std::vector<TriggeredEffectConfig_t> ParseTriggeredEffects(const nlohmann::json& rContainerJson,
                                                           const std::string& rKey,
                                                           const std::string& rSourceId)
{
    std::vector<TriggeredEffectConfig_t> effects;
    if (!rContainerJson.contains(rKey))
    {
        return effects;
    }
    const nlohmann::json& rArray = rContainerJson.at(rKey);
    if (!rArray.is_array())
    {
        throw std::runtime_error("'" + rKey + "' on '" + rSourceId + "' must be a JSON array");
    }
    for (const auto& rEffectJson : rArray)
    {
        try
        {
            effects.push_back(ParseTriggeredEffectConfig(rEffectJson, rKey));
        }
        catch (const std::exception& rEx)
        {
            throw std::runtime_error("On '" + rSourceId + "': " + rEx.what());
        }
    }
    return effects;
}

} // namespace TriggeredEffectParser
} // namespace ac
