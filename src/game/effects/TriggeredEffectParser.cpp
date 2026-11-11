#include "game/effects/TriggeredEffectParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/Rational.h"
#include "lib/config/ConfigReader.h"
#include "lib/config/EnumNames.h"

#include <magic_enum.hpp>

#include <algorithm>
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

using ParseFn_ = std::function<void(const ConfigReader&, TriggeredEffectConfig_t&)>;

std::vector<std::string> RequireNonEmptyStringArray_(const ConfigReader& rParams,
                                                     const char* pKey)
{
    rParams.NonEmptyArray(pKey);
    std::vector<std::string> values = rParams.StringArray(pKey);
    if (std::ranges::contains(values, std::string()))
    {
        rParams.FailKey(pKey, "entries must be non-empty strings");
    }
    return values;
}

void ParseAddBuilding_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = AddBuildingEffect_t{rParams.NonEmptyString("building_id")};
}

void ParseGrantTech_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rParams.RejectUnknownKeys({"tech_id"});
    GrantTechEffect_t grant;
    if (rParams.Has("tech_id"))
    {
        grant.techId = rParams.NonEmptyString("tech_id");
    }
    rEffect.effect = grant;
}

void ParseGrantUnit_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasComponents = rParams.Has("component_ids");
    const bool bHasDesignId = rParams.Has("design_id");
    if (bHasComponents == bHasDesignId)
    {
        throw std::runtime_error(
            "GrantUnit requires exactly one of 'component_ids' or 'design_id'");
    }
    GrantUnitEffect_t grant;
    if (bHasComponents)
    {
        grant.componentIds = RequireNonEmptyStringArray_(rParams, "component_ids");
    }
    else
    {
        grant.designId = rParams.NonEmptyString("design_id");
    }
    grant.count = rParams.Has("count") ? rParams.IntAtLeast("count", 1) : 1;
    rEffect.effect = grant;
}

void ParseGrantEnergy_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasAmount = rParams.Has("amount");
    const bool bHasFormula = rParams.Has("formula");
    if (bHasAmount == bHasFormula)
    {
        throw std::runtime_error("GrantEnergy requires exactly one of 'amount' or 'formula'");
    }
    GrantEnergyEffect_t grant;
    if (bHasAmount)
    {
        grant.amount = rParams.IntAtLeast("amount", 0);
    }
    else
    {
        grant.formula = rParams.NonEmptyString("formula");
    }
    rEffect.effect = grant;
}

void ParseAddOccupant_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = AddOccupantEffect_t{rParams.NonEmptyString("improvement_id")};
}

void ParseRevealMap_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    RevealMapEffect_t reveal;
    reveal.radius = rParams.IntAtLeast("radius", 0);
    rEffect.effect = reveal;
}

void ParseTeleportUnit_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    TeleportUnitEffect_t teleport;
    teleport.minDistance = rParams.IntAtLeast("min_distance", 0);
    teleport.maxDistance = rParams.IntAtLeast("max_distance", teleport.minDistance);
    rEffect.effect = teleport;
}

void ParseFinishProduction_(const ConfigReader&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = FinishProductionEffect_t{};
}

void ParseCloneUnit_(const ConfigReader&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = CloneUnitEffect_t{};
}

void ParseGrantCommlink_(const ConfigReader&, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = GrantCommlinkEffect_t{};
}

void ParseSpawnNative_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    SpawnNativeEffect_t spawn;
    spawn.nativeId = rParams.NonEmptyString("native_id");
    if (rParams.Has("cargo_ids"))
    {
        spawn.cargoIds =
            RequireNonEmptyStringArray_(rParams, "cargo_ids");
    }
    rEffect.effect = spawn;
}

std::vector<Rational_t> RequireChanceArray_(const ConfigReader& rParams)
{
    std::vector<Rational_t> chances;
    for (const auto& rEntry : rParams.NonEmptyArray("chance"))
    {
        const Rational_t chance = Rational_t::ParseJson(rEntry);
        if (chance.numerator < 0)
        {
            rParams.FailKey("chance", "numerators must be >= 0");
        }
        chances.push_back(chance);
    }
    return chances;
}

void ParseTerraformCluster_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    TerraformClusterEffect_t cluster;
    cluster.improvementIds = RequireNonEmptyStringArray_(rParams, "improvement_ids");
    cluster.chance = RequireChanceArray_(rParams);
    rEffect.effect = cluster;
}

void ParseAddRandomOccupant_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    AddRandomOccupantEffect_t add;
    add.improvementIds = RequireNonEmptyStringArray_(rParams, "improvement_ids");
    rEffect.effect = add;
}
// TODO: We may need to refactor this concept once we add Supply Pods
void ParseGrantResource_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    GrantResourceEffect_t grant;
    grant.resource = ParseStatId(rParams.StringOr("resource", ""));
    if (grant.resource != StatId_t::Minerals && grant.resource != StatId_t::Nutrients)
    {
        throw std::runtime_error(
            "GrantResource 'resource' must be minerals or nutrients (energy treasury uses "
            "GrantEnergy)");
    }
    grant.amount = rParams.Int("amount");
    if (rParams.Has("stat"))
    {
        const StatId_t modifier = ParseStatId(rParams.String("stat"));
        if (DomainFor(modifier) != ResolveDomain_t::Base)
        {
            throw std::runtime_error(
                "GrantResource 'stat' must be a base-domain stat (resolved on the subject base)");
        }
        grant.modifierStat = modifier;
    }
    rEffect.effect = grant;
}

void ParseWorldParameter_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    WorldParameterEffect_t world;
    world.parameter = ParseWorldParameterId(rParams.StringOr("parameter", ""));
    world.amount = rParams.Int("amount");
    rEffect.effect = world;
}

void ParseSetInfiltration_(const ConfigReader& /*rParams*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = SetInfiltrationEffect_t{};
}

void ParseModifyPopulation_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    ModifyPopulationEffect_t modify;
    modify.amount = rParams.Int("amount");
    modify.op = EffectConfigParser::ParseModifierOp(rParams.StringOr("op", "Add"));
    if (modify.op != ModifierOp_t::Add && modify.op != ModifierOp_t::AddPercent)
    {
        throw std::runtime_error("ModifyPopulation op must be Add or AddPercent");
    }
    modify.minSize = rParams.Has("min_size") ? rParams.IntAtLeast("min_size", 0) : 0;
    rEffect.effect = modify;
}

void ParseGrantXp_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    GrantXpEffect_t grant;
    grant.amount = rParams.Int("amount");
    grant.op = EffectConfigParser::ParseModifierOp(rParams.StringOr("op", "Add"));
    if (rParams.Has("remove_host_chance"))
    {
        grant.removeHostChance = rParams.Rational("remove_host_chance");
    }
    rEffect.effect = grant;
}

void ParseRestoreHitPoints_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    RestoreHitPointsEffect_t restore;
    restore.amount = rParams.Int("amount");
    const std::string opStr = rParams.StringOr("op", "Add");
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

void ParseDestroyFacility_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    DestroyFacilityEffect_t destroy;
    destroy.count = rParams.IntAtLeast("count", 1);
    destroy.exclusions.excludeHq = rParams.Bool("exclude_hq");
    destroy.exclusions.excludeSecretProjects = rParams.Bool("exclude_secret_projects");
    rEffect.effect = destroy;
}

void ParseRebel_(const ConfigReader& /*rParams*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = RebelEffect_t{};
}

void ParseDestroyUnit_(const ConfigReader& /*rParams*/, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = DestroyUnitEffect_t{};
}

void ParseEarthquake_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasLevels = rParams.Has("levels");
    const bool bHasStat = rParams.Has("levels_stat");
    if (bHasLevels == bHasStat)
    {
        throw std::runtime_error(
            "Earthquake requires exactly one of 'levels' (a fixed count) or 'levels_stat' "
            "(resolved off the subject unit)");
    }

    EarthquakeEffect_t quake;
    if (bHasLevels)
    {
        quake.levels = rParams.IntAtLeast("levels", 1);
    }
    else
    {
        const StatId_t stat = ParseStatId(rParams.String("levels_stat"));
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

void ParseSetTerrainFeature_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = SetTerrainFeatureEffect_t{rParams.NonEmptyString("feature"),
                                              rParams.BoolOr("present", true)};
}

void ParseStepRockiness_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const int steps = rParams.Int("steps");
    if (steps == 0)
    {
        throw std::runtime_error("StepRockiness 'steps' must be non-zero: negative smooths "
                                 "toward Flat, positive roughens toward Rocky");
    }
    rEffect.effect = StepRockinessEffect_t{steps};
}

void ParseElevationChange_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const std::string direction = rParams.String("direction");
    if (direction != "raise" && direction != "lower")
    {
        rParams.FailKey("direction", "must be 'raise' or 'lower', not '" + direction + "'");
    }
    rEffect.effect = ElevationChangeEffect_t{direction == "raise"};
}

// Severities are a closed enum, so an unknown name fails here rather than at a later lookup.
void ParseCommitAtrocity_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    CommitAtrocityEffect_t committed;
    committed.severity = rParams.Enum<AtrocitySeverityId_t>("severity", "atrocity severity");
    rEffect.effect = committed;
}

void ParseRecordMindControl_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = RecordMindControlEffect_t{rParams.IntAtLeast("weight", 1)};
}

void ParseGrantCleanMinerals_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    GrantCleanMineralsEffect_t grant;
    grant.amount = rParams.IntAtLeast("amount", 1);
    grant.bRequiresFirstBloom = rParams.Bool("requires_first_bloom");
    rEffect.effect = grant;
}

void ParseAddVirtualMinerals_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    rEffect.effect = AddVirtualMineralsEffect_t{rParams.IntAtLeast("amount", 1)};
}

void ParseFungalBloom_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasTiles = rParams.Has("tiles");
    const bool bHasStat = rParams.Has("tiles_stat");
    const bool bHasChance = rParams.Has("chance");
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
        bloom.tiles = rParams.IntAtLeast("tiles", 1);
    }
    else if (bHasStat)
    {
        const StatId_t stat = ParseStatId(rParams.String("tiles_stat"));
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
        bloom.chance = RequireChanceArray_(rParams);
    }
    rEffect.effect = bloom;
}

void ParseExplosion_(const ConfigReader& rParams, TriggeredEffectConfig_t& rEffect)
{
    const bool bHasRadius = rParams.Has("radius");
    const bool bHasStat = rParams.Has("radius_stat");
    if (bHasRadius == bHasStat)
    {
        throw std::runtime_error(
            "Explosion requires exactly one of 'radius' (a fixed count) or 'radius_stat' "
            "(resolved off the subject unit)");
    }

    ExplosionEffect_t explosion;
    if (bHasRadius)
    {
        explosion.radius = rParams.IntAtLeast("radius", 1);
    }
    else
    {
        const StatId_t stat = ParseStatId(rParams.String("radius_stat"));
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

    const nlohmann::json parameters = effectJson.value("parameters", nlohmann::json::object());
    rParseParameters(ConfigReader(parameters, typeStr + " parameters"), effect);
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
