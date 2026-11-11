#include "game/production/ProductionConfigParser.h"
#include "game/production/ScrapConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ConstructableKind.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

HurryKindConfig_t ParseHurry_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"formula", "mineral_threshold", "below_threshold_multiplier"});
    HurryKindConfig_t kind;
    kind.formula = rReader.NonEmptyString("formula");
    kind.mineralThreshold = rReader.IntAtLeast("mineral_threshold", 0);
    kind.belowThresholdMultiplier = rReader.IntAtLeast("below_threshold_multiplier", 1);
    return kind;
}

// Stockpile is rejected, since it never completes.
ConstructableKind_t ParseKindId_(const ConfigReader& rKinds, const std::string& rKind)
{
    if (rKind.empty())
    {
        rKinds.Fail("kind must be a non-empty string");
    }
    ConstructableKind_t kindId = ConstructableKind_t::Building;
    try
    {
        kindId = ParseConstructableKind(rKind);
    }
    catch (const std::runtime_error&)
    {
        rKinds.FailKey(rKind, "is not a known constructable kind");
    }
    if (kindId == ConstructableKind_t::Stockpile)
    {
        rKinds.FailKey(rKind, "cannot be hurried or scrapped; omit it from kinds");
    }
    return kindId;
}

ScrapKindConfig_t ParseDefaultScrap_(ConstructableKind_t kindId, const ConfigReader& rKind)
{
    if (kindId == ConstructableKind_t::SecretProject)
    {
        rKind.FailKey("default_scrap",
                      "cannot be configured; secret projects cannot be scrapped");
    }
    return ScrapConfigParser::ParseKindConfig(rKind.Object("default_scrap"));
}

ProductionKindConfig_t ParseKind_(ConstructableKind_t kindId, const ConfigReader& rKind)
{
    rKind.RejectUnknownKeys({"hurry", "default_scrap"});
    ProductionKindConfig_t kindConfig;
    if (rKind.Has("hurry"))
    {
        kindConfig.hurry = ParseHurry_(rKind.Object("hurry"));
    }
    if (rKind.Has("default_scrap"))
    {
        kindConfig.defaultScrap = ParseDefaultScrap_(kindId, rKind);
    }
    if (!kindConfig.hurry && !kindConfig.defaultScrap)
    {
        rKind.Fail("must include hurry or default_scrap");
    }
    return kindConfig;
}

// An empty object turns hurry and scrap off.
decltype(ProductionConfig_t::kinds) ParseKinds_(const ConfigReader& rKinds)
{
    decltype(ProductionConfig_t::kinds) kinds;
    for (const auto& [rKind, rUnused] : rKinds.Json().items())
    {
        const ConstructableKind_t kindId = ParseKindId_(rKinds, rKind);
        kinds.emplace(kindId, ParseKind_(kindId, rKinds.Object(rKind)));
    }
    return kinds;
}

ProductionConfig_t ParseProduction_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"retool_penalty_threshold", "retool_penalty_percent",
                               "prototype_surcharge_percent", "kinds", "effects",
                               "on_unit_produced_effects"});
    ProductionConfig_t config;
    config.retoolPenaltyThreshold = rReader.IntAtLeast("retool_penalty_threshold", 0);
    config.retoolPenaltyPercent = rReader.IntInRange("retool_penalty_percent", 0, 100);
    // No upper bound: a prototype that costs triple is a coherent mod
    // choice, unlike a retool penalty over 100%.
    config.prototypeSurchargePercent = rReader.IntAtLeast("prototype_surcharge_percent", 0);
    config.kinds = ParseKinds_(rReader.Object("kinds"));
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::Production, "production");
    config.onUnitProducedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_unit_produced_effects", "production");
    return config;
}

} // namespace

ProductionConfig_t ProductionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<ProductionConfig_t>(configPath, "production",
                                                                ParseProduction_);
}

} // namespace ac
