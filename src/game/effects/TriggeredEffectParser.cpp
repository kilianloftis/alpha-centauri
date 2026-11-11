#include "game/effects/TriggeredEffectParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/Rational.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"

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

int ParsePositiveInt_(const nlohmann::json& parameters, const char* pKey, const char* pType)
{
    if (!parameters.contains(pKey) || !parameters.at(pKey).is_number_integer())
    {
        throw std::runtime_error(std::string(pType) + " '" + pKey + "' must be an integer");
    }
    const int value = parameters.at(pKey).get<int>();
    if (value < 1)
    {
        throw std::runtime_error(std::string(pType) + " '" + pKey + "' must be >= 1");
    }
    return value;
}

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
    ConfigFields::RejectUnknownKeys(parameters, {"tech_id"}, "GrantTech");
    GrantTechEffect_t grant;
    if (parameters.contains("tech_id"))
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
    rEffect.effect = grant;
}

void ParseGrantUnit_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasComponents = parameters.contains("component_ids");
    const bool bHasDesignId = parameters.contains("design_id");
    if (bHasComponents == bHasDesignId)
    {
        throw std::runtime_error(
            "GrantUnit requires exactly one of 'component_ids' or 'design_id'");
    }
    GrantUnitEffect_t grant;
    if (bHasComponents)
    {
        if (!parameters.at("component_ids").is_array() || parameters.at("component_ids").empty())
        {
            throw std::runtime_error("GrantUnit 'component_ids' must be a non-empty array");
        }
        for (const auto& rId : parameters.at("component_ids"))
        {
            if (!rId.is_string() || rId.get<std::string>().empty())
            {
                throw std::runtime_error(
                    "GrantUnit 'component_ids' entries must be non-empty strings");
            }
            grant.componentIds.push_back(rId.get<std::string>());
        }
    }
    else
    {
        if (!parameters.at("design_id").is_string()
            || parameters.at("design_id").get<std::string>().empty())
        {
            throw std::runtime_error("GrantUnit 'design_id' must be a non-empty string");
        }
        grant.designId = parameters.at("design_id").get<std::string>();
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
    const bool bHasAmount = parameters.contains("amount");
    const bool bHasFormula = parameters.contains("formula");
    if (bHasAmount == bHasFormula)
    {
        throw std::runtime_error("GrantEnergy requires exactly one of 'amount' or 'formula'");
    }
    GrantEnergyEffect_t grant;
    if (bHasAmount)
    {
        const int amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
        if (amount < 0)
        {
            throw std::runtime_error("GrantEnergy 'amount' must be >= 0");
        }
        grant.amount = amount;
    }
    else
    {
        if (!parameters.at("formula").is_string() || parameters.at("formula").get<std::string>().empty())
        {
            throw std::runtime_error("GrantEnergy 'formula' must be a non-empty string");
        }
        grant.formula = parameters.at("formula").get<std::string>();
    }
    rEffect.effect = grant;
}

void ParseAddOccupant_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const std::string improvementId = parameters.value("improvement_id", "");
    if (improvementId.empty())
    {
        throw std::runtime_error("AddOccupant requires a non-empty 'improvement_id'");
    }
    rEffect.effect = AddOccupantEffect_t{improvementId};
}

void ParseRevealMap_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    RevealMapEffect_t reveal;
    reveal.radius = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "radius"));
    if (reveal.radius < 0)
    {
        throw std::runtime_error("RevealMap 'radius' must be >= 0");
    }
    rEffect.effect = reveal;
}

void ParseTeleportUnit_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    TeleportUnitEffect_t teleport;
    teleport.minDistance =
        static_cast<int>(EffectConfigParser::RequireNumber(parameters, "min_distance"));
    teleport.maxDistance =
        static_cast<int>(EffectConfigParser::RequireNumber(parameters, "max_distance"));
    if (teleport.minDistance < 0 || teleport.maxDistance < teleport.minDistance)
    {
        throw std::runtime_error(
            "TeleportUnit requires 0 <= min_distance <= max_distance");
    }
    rEffect.effect = teleport;
}

void ParseFinishProduction_(const nlohmann::json&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = FinishProductionEffect_t{};
}

void ParseCloneUnit_(const nlohmann::json&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = CloneUnitEffect_t{};
}

void ParseGrantCommlink_(const nlohmann::json&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = GrantCommlinkEffect_t{};
}

std::vector<std::string> RequireNonEmptyStringArray_(const nlohmann::json& parameters,
                                                     const char* pKey, const char* pType)
{
    if (!parameters.contains(pKey) || !parameters.at(pKey).is_array()
        || parameters.at(pKey).empty())
    {
        throw std::runtime_error(std::string(pType) + " requires a non-empty '" + pKey
                                 + "' array");
    }
    std::vector<std::string> values;
    for (const auto& rEntry : parameters.at(pKey))
    {
        if (!rEntry.is_string() || rEntry.get<std::string>().empty())
        {
            throw std::runtime_error(std::string(pType) + " '" + pKey
                                     + "' entries must be non-empty strings");
        }
        values.push_back(rEntry.get<std::string>());
    }
    return values;
}

void ParseSpawnNative_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    SpawnNativeEffect_t spawn;
    spawn.nativeId = parameters.value("native_id", "");
    if (spawn.nativeId.empty())
    {
        throw std::runtime_error("SpawnNative requires a non-empty 'native_id'");
    }
    if (parameters.contains("cargo_ids"))
    {
        spawn.cargoIds =
            RequireNonEmptyStringArray_(parameters, "cargo_ids", "SpawnNative");
    }
    rEffect.effect = spawn;
}

std::vector<Rational_t> RequireChanceArray_(const nlohmann::json& parameters, const char* pType)
{
    if (!parameters.contains("chance") || !parameters.at("chance").is_array()
        || parameters.at("chance").empty())
    {
        throw std::runtime_error(std::string(pType) + " requires a non-empty 'chance' array");
    }
    std::vector<Rational_t> chances;
    for (const auto& rEntry : parameters.at("chance"))
    {
        const Rational_t chance = Rational_t::ParseJson(rEntry);
        if (chance.numerator < 0)
        {
            throw std::runtime_error(std::string(pType) + " 'chance' numerator must be >= 0");
        }
        chances.push_back(chance);
    }
    return chances;
}

void ParseTerraformCluster_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    TerraformClusterEffect_t cluster;
    cluster.improvementIds =
        RequireNonEmptyStringArray_(parameters, "improvement_ids", "TerraformCluster");
    cluster.chance = RequireChanceArray_(parameters, "TerraformCluster");
    rEffect.effect = cluster;
}

void ParseAddRandomOccupant_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    AddRandomOccupantEffect_t add;
    add.improvementIds =
        RequireNonEmptyStringArray_(parameters, "improvement_ids", "AddRandomOccupant");
    rEffect.effect = add;
}
// TODO: We may need to refactor this concept once we add Supply Pods
void ParseGrantResource_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    GrantResourceEffect_t grant;
    grant.resource = ParseStatId(parameters.value("resource", ""));
    if (grant.resource != StatId_t::Minerals && grant.resource != StatId_t::Nutrients)
    {
        throw std::runtime_error(
            "GrantResource 'resource' must be minerals or nutrients (energy treasury uses "
            "GrantEnergy)");
    }
    grant.amount = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "amount"));
    if (parameters.contains("stat"))
    {
        const StatId_t modifier = ParseStatId(parameters.at("stat").get<std::string>());
        if (DomainFor(modifier) != ResolveDomain_t::Base)
        {
            throw std::runtime_error(
                "GrantResource 'stat' must be a base-domain stat (resolved on the subject base)");
        }
        grant.modifierStat = modifier;
    }
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
    destroy.count = ParsePositiveInt_(parameters, "count", "DestroyFacility");
    destroy.exclusions.excludeHq = requireBool("exclude_hq");
    destroy.exclusions.excludeSecretProjects = requireBool("exclude_secret_projects");
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

void ParseSetTerrainFeature_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    if (!parameters.contains("feature") || !parameters.at("feature").is_string()
        || parameters.at("feature").get<std::string>().empty())
    {
        throw std::runtime_error(
            "SetTerrainFeature requires a non-empty 'feature' terrain occupant id");
    }
    if (parameters.contains("present") && !parameters.at("present").is_boolean())
    {
        throw std::runtime_error("SetTerrainFeature 'present' must be a boolean");
    }
    rEffect.effect = SetTerrainFeatureEffect_t{parameters.at("feature").get<std::string>(),
                                              parameters.value("present", true)};
}

void ParseStepRockiness_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const int steps = static_cast<int>(EffectConfigParser::RequireNumber(parameters, "steps"));
    if (steps == 0)
    {
        throw std::runtime_error("StepRockiness 'steps' must be non-zero: negative smooths "
                                 "toward Flat, positive roughens toward Rocky");
    }
    rEffect.effect = StepRockinessEffect_t{steps};
}

void ParseElevationChange_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    if (!parameters.contains("direction") || !parameters.at("direction").is_string())
    {
        throw std::runtime_error("ElevationChange requires a 'direction' of 'raise' or 'lower'");
    }
    const std::string direction = parameters.at("direction").get<std::string>();
    if (direction != "raise" && direction != "lower")
    {
        throw std::runtime_error("ElevationChange 'direction' must be 'raise' or 'lower', not '"
                                 + direction + "'");
    }
    rEffect.effect = ElevationChangeEffect_t{direction == "raise"};
}

// Severities are a closed enum, so an unknown name fails here rather than at a later lookup.
void ParseCommitAtrocity_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    if (!parameters.contains("severity") || !parameters.at("severity").is_string())
    {
        throw std::runtime_error("CommitAtrocity requires a string 'severity'");
    }
    const std::string severity = parameters.at("severity").get<std::string>();
    if (severity.empty())
    {
        throw std::runtime_error("CommitAtrocity 'severity' must not be empty");
    }
    CommitAtrocityEffect_t committed;
    committed.severity = EnumFromName<AtrocitySeverityId_t>(severity, "atrocity severity");
    rEffect.effect = committed;
}

void ParseRecordMindControl_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect =
        RecordMindControlEffect_t{ParsePositiveInt_(parameters, "weight", "RecordMindControl")};
}

void ParseGrantCleanMinerals_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    GrantCleanMineralsEffect_t grant;
    grant.amount = ParsePositiveInt_(parameters, "amount", "GrantCleanMinerals");
    if (!parameters.contains("requires_first_bloom")
        || !parameters.at("requires_first_bloom").is_boolean())
    {
        throw std::runtime_error("GrantCleanMinerals 'requires_first_bloom' must be a boolean");
    }
    grant.bRequiresFirstBloom = parameters.at("requires_first_bloom").get<bool>();
    rEffect.effect = grant;
}

void ParseAddVirtualMinerals_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect =
        AddVirtualMineralsEffect_t{ParsePositiveInt_(parameters, "amount", "AddVirtualMinerals")};
}

void ParseFungalBloom_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasTiles = parameters.contains("tiles");
    const bool bHasStat = parameters.contains("tiles_stat");
    const bool bHasChance = parameters.contains("chance");
    const int modeCount = static_cast<int>(bHasTiles) + static_cast<int>(bHasStat)
                          + static_cast<int>(bHasChance);
    if (modeCount != 1)
    {
        throw std::runtime_error(
            "FungalBloom requires exactly one of 'tiles' (a fixed count), 'tiles_stat' "
            "(resolved off the subject unit), or 'chance' (per-distance Chebyshev rolls)");
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
    else if (bHasStat)
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
    else
    {
        bloom.chance = RequireChanceArray_(parameters, "FungalBloom");
    }
    rEffect.effect = bloom;
}

void ParseExplosion_(const nlohmann::json& parameters, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasRadius = parameters.contains("radius");
    const bool bHasStat = parameters.contains("radius_stat");
    if (bHasRadius == bHasStat)
    {
        throw std::runtime_error(
            "Explosion requires exactly one of 'radius' (a fixed count) or 'radius_stat' "
            "(resolved off the subject unit)");
    }

    ExplosionEffect_t explosion;
    if (bHasRadius)
    {
        explosion.radius =
            static_cast<int>(EffectConfigParser::RequireNumber(parameters, "radius"));
        if (explosion.radius < 1)
        {
            throw std::runtime_error("Explosion 'radius' must be >= 1");
        }
    }
    else
    {
        if (!parameters.at("radius_stat").is_string())
        {
            throw std::runtime_error("Explosion 'radius_stat' must be a string");
        }
        const StatId_t stat = ParseStatId(parameters.at("radius_stat").get<std::string>());
        if (DomainFor(stat) != ResolveDomain_t::Unit)
        {
            throw std::runtime_error(
                "Explosion 'radius_stat' must be a unit stat: the size is resolved off "
                "the unit the trigger stamped");
        }
        explosion.radiusStat = stat;
    }
    rEffect.effect = explosion;
}

const std::unordered_map<std::string, ParseFn_>& TypeParsers_()
{
    static const std::unordered_map<std::string, ParseFn_> k_Parsers = {
        {"AddBuilding", ParseAddBuilding_},
        {"GrantTech", ParseGrantTech_},
        {"GrantUnit", ParseGrantUnit_},
        {"GrantEnergy", ParseGrantEnergy_},
        {"GrantResource", ParseGrantResource_},
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
        {"Explosion", ParseExplosion_},
        {"SetTerrainFeature", ParseSetTerrainFeature_},
        {"StepRockiness", ParseStepRockiness_},
        {"ElevationChange", ParseElevationChange_},
        {"CommitAtrocity", ParseCommitAtrocity_},
        {"RecordMindControl", ParseRecordMindControl_},
        {"GrantCleanMinerals", ParseGrantCleanMinerals_},
        {"AddVirtualMinerals", ParseAddVirtualMinerals_},
        {"AddOccupant", ParseAddOccupant_},
        {"RevealMap", ParseRevealMap_},
        {"TeleportUnit", ParseTeleportUnit_},
        {"FinishProduction", ParseFinishProduction_},
        {"CloneUnit", ParseCloneUnit_},
        {"GrantCommlink", ParseGrantCommlink_},
        {"SpawnNative", ParseSpawnNative_},
        {"TerraformCluster", ParseTerraformCluster_},
        {"AddRandomOccupant", ParseAddRandomOccupant_},
    };
    return k_Parsers;
}

const ParseFn_& FindTypeParser_(const std::string& rTypeStr, const std::string& rListName)
{
    const auto& parsers = TypeParsers_();
    const auto it = parsers.find(rTypeStr);
    if (it != parsers.end())
    {
        return it->second;
    }
    // The continuous parser's own table, so a new continuous type is reported as belonging
    // in `effects`.
    if (EffectConfigParser::IsEffectType(rTypeStr))
    {
        throw std::runtime_error(
            "'" + rTypeStr + "' is a continuous effect and belongs in 'effects', not '"
            + rListName + "'; a triggered list holds one-shot effects only");
    }
    throw std::runtime_error("Unknown triggered effect type: '" + rTypeStr + "' in '"
                             + rListName + "'");
}

// Timing comes from the list and resolution from an explicit context, so the continuous-only
// keys are rejected.
void RejectContinuousOnlyKeys_(const nlohmann::json& rEffectJson, const std::string& rTypeStr,
                               const std::string& rListName)
{
    for (const char* pKey : {"scope", "persistence", "radius", "min_radius",
                             "buildingFilter", "removed_by_tech"})
    {
        if (rEffectJson.contains(pKey))
        {
            throw std::runtime_error(std::string("Triggered effect '") + rTypeStr + "' in '"
                                     + rListName + "' cannot carry '" + pKey
                                     + "': it fires when its list's trigger fires, against that "
                                       "trigger's subject");
        }
    }
}

OncePer_t ParseOncePer_(const nlohmann::json& onceJson)
{
    OncePer_t oncePer;
    // Required: which subject keeps the key decides whether the entry can fire from a trigger at
    // all.
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

TriggeredEffectConfig_t ParseTriggeredEffectConfig(const nlohmann::json& effectJson,
                                                   const std::string& rListName)
{
    TriggeredEffectConfig_t effect;
    const std::string typeStr = effectJson.at("type").get<std::string>();
    const ParseFn_& rParseParameters = FindTypeParser_(typeStr, rListName);

    if (effectJson.contains("factionFilter"))
    {
        // Only SetInfiltration picks its targets; on any other type a filter would change nothing.
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
    RejectContinuousOnlyKeys_(effectJson, typeStr, rListName);

    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());
    rParseParameters(parameters, effect);
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
