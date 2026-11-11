#include "game/population/PopCompositionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>
#include <vector>

namespace ac
{

namespace
{

std::vector<EffectConfig_t> ParseEffectArray_(const ConfigReader& rReader, const char* pKey,
                                              const std::string& rSourceId)
{
    std::vector<EffectConfig_t> effects;
    for (const auto& rEffectJson : rReader.Array(pKey))
    {
        EffectConfig_t effect = EffectConfigParser::ParseEffectConfig(rEffectJson);
        // BaseLocal, not PopComposition: these arrays are collected against the rioting base,
        // so ThisBase is legal here and stays rejected on the faction-wide `effects` array.
        EffectConfigParser::ValidateEffectForSource(
            effect, EffectSourceKind_t::PopCompositionBaseLocal, rSourceId);
        effects.push_back(std::move(effect));
    }
    return effects;
}

RiotTier_t ParseRiotTier_(const ConfigReader& rReader)
{
    RiotTier_t tier;
    tier.minTurns = rReader.IntAtLeast("min_turns", 1);
    if (rReader.Has("effects"))
    {
        tier.effects =
            ParseEffectArray_(rReader, "effects", "pop_composition.riot_tiers.effects");
    }
    tier.onEnterEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_enter_effects", "pop_composition.riot_tiers");
    return tier;
}

std::vector<RiotTier_t> ParseRiotTiers_(const ConfigReader& rReader)
{
    std::vector<RiotTier_t> tiers;
    for (const ConfigReader& rTier : rReader.Objects("riot_tiers"))
    {
        tiers.push_back(ParseRiotTier_(rTier));
    }
    return tiers;
}

RebelDistanceMode_t ParseRequiredDistanceMode_(const ConfigReader& rReader)
{
    const std::string modeWire = rReader.String("distance_mode");
    const std::optional<RebelDistanceMode_t> mode = ParseRebelDistanceMode(modeWire);
    if (!mode)
    {
        rReader.FailKey("distance_mode", "must be none, nearest_base, hq_distance, or "
                                         "nearby_bases, got '" + modeWire + "'");
    }
    return *mode;
}

// Required, every field: these are the rebellion's tuning numbers.
RebelSelectionConfig_t ParseRebelSelection_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"distance_mode", "fade_radius", "distance_weight_per_tile",
                               "base_join_weight", "missing_hq_distance"});
    RebelSelectionConfig_t selection;
    selection.distanceMode = ParseRequiredDistanceMode_(rReader);
    selection.fadeRadius = rReader.IntAtLeast("fade_radius", 0);
    selection.distanceWeightPerTile = rReader.IntAtLeast("distance_weight_per_tile", 0);
    selection.baseJoinWeight = rReader.IntAtLeast("base_join_weight", 0);
    selection.missingHqDistance = rReader.IntAtLeast("missing_hq_distance", 0);
    return selection;
}

PopCompositionConfig_t ParseComposition_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "bureaucracy_limit_formula", "bureaucracy_drone_formula",
        "size_drone_formula", "occupation_drone_formula",
        "drone_type", "talent_type",
        "assimilation_drones", "assimilation_decay_turns",
        "riot_threshold", "golden_age_threshold",
        "effects", "golden_age_effects", "riot_tiers", "rebel_selection",
    });
    PopCompositionConfig_t config;
    config.bureaucracyLimitFormula = rReader.NonEmptyString("bureaucracy_limit_formula");
    config.bureaucracyDroneFormula = rReader.NonEmptyString("bureaucracy_drone_formula");
    config.sizeDroneFormula = rReader.NonEmptyString("size_drone_formula");
    config.occupationDroneFormula = rReader.NonEmptyString("occupation_drone_formula");
    config.droneTypeId = rReader.NonEmptyString("drone_type");
    config.talentTypeId = rReader.NonEmptyString("talent_type");
    config.assimilationDrones = rReader.IntAtLeast("assimilation_drones", 1);
    config.assimilationDecayTurns = rReader.IntAtLeast("assimilation_decay_turns", 1);
    config.riotThreshold = rReader.Int("riot_threshold");
    config.goldenAgeThreshold = rReader.Int("golden_age_threshold");
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::PopComposition, "pop_composition");
    if (rReader.Has("golden_age_effects"))
    {
        config.goldenAgeEffects = ParseEffectArray_(rReader, "golden_age_effects",
                                                    "pop_composition.golden_age_effects");
    }
    if (rReader.Has("riot_tiers"))
    {
        config.riotTiers = ParseRiotTiers_(rReader);
    }
    config.rebelSelection = ParseRebelSelection_(rReader.Object("rebel_selection"));
    return config;
}

} // namespace

std::optional<RebelDistanceMode_t> ParseRebelDistanceMode(const std::string& rMode)
{
    for (const RebelDistanceMode_t mode : {RebelDistanceMode_t::None,
                                           RebelDistanceMode_t::NearestBase,
                                           RebelDistanceMode_t::HqDistance,
                                           RebelDistanceMode_t::NearbyBases})
    {
        if (rMode == RebelDistanceModeWireName(mode))
        {
            return mode;
        }
    }
    return std::nullopt;
}

const RiotTier_t* FindActiveRiotTier(const PopCompositionConfig_t& rConfig, int consecutiveTurns)
{
    const RiotTier_t* pBest = nullptr;
    for (const RiotTier_t& rTier : rConfig.riotTiers)
    {
        if (consecutiveTurns >= rTier.minTurns)
        {
            if (!pBest || rTier.minTurns > pBest->minTurns)
            {
                pBest = &rTier;
            }
        }
    }
    return pBest;
}

PopCompositionConfig_t PopCompositionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<PopCompositionConfig_t>(
        configPath, "pop composition", ParseComposition_);
}

} // namespace ac
