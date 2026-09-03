#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

std::vector<EffectConfig_t> ParseEffectArray_(const nlohmann::json& rArray,
                                              const std::string& rPath, const auto& fail)
{
    if (!rArray.is_array())
    {
        fail("'" + rPath + "' must be a JSON array");
    }
    std::vector<EffectConfig_t> effects;
    for (const auto& rEffectJson : rArray)
    {
        EffectConfig_t effect = EffectConfigParser::ParseEffectConfig(rEffectJson);
        // BaseLocal, not PopComposition: these arrays are collected against the rioting base,
        // so ThisBase is legal here and stays rejected on the faction-wide `effects` array.
        EffectConfigParser::ValidateEffectForSource(
            effect, EffectSourceKind_t::PopCompositionBaseLocal, rPath);
        effects.push_back(std::move(effect));
    }
    return effects;
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
        configPath, "pop composition", [&configPath](const nlohmann::json& rJson) {
            const auto fail = [&](const std::string& rMessage) {
                throw std::runtime_error("Pop composition config '" + configPath + "': "
                                         + rMessage);
            };

            static const std::vector<std::string> knownKeys = {
                "bureaucracy_limit_formula", "bureaucracy_drone_formula",
                "size_drone_formula", "occupation_drone_formula",
                "drone_type", "talent_type",
                "assimilation_drones", "assimilation_decay_turns",
                "riot_threshold", "golden_age_threshold",
                "effects", "golden_age_effects", "riot_tiers", "rebel_selection",
            };
            for (const auto& [rKey, rUnused] : rJson.items())
            {
                if (std::find(knownKeys.begin(), knownKeys.end(), rKey) == knownKeys.end())
                {
                    fail("unknown key '" + rKey + "'");
                }
            }

            const auto requireNonEmptyString = [&](const char* key) {
                if (!rJson.contains(key) || !rJson.at(key).is_string())
                {
                    fail(std::string("'") + key + "' must be a non-empty string");
                }
                const std::string value = rJson.at(key).get<std::string>();
                if (value.empty())
                {
                    fail(std::string("'") + key + "' must be a non-empty string");
                }
                return value;
            };

            const auto requirePositiveInt = [&](const char* key) {
                if (!rJson.contains(key) || !rJson.at(key).is_number_integer())
                {
                    fail(std::string("'") + key + "' must be a positive integer");
                }
                const int value = rJson.at(key).get<int>();
                if (value <= 0)
                {
                    fail(std::string("'") + key + "' must be > 0, got "
                         + std::to_string(value));
                }
                return value;
            };

            const auto requireInt = [&](const char* key) {
                if (!rJson.contains(key) || !rJson.at(key).is_number_integer())
                {
                    fail(std::string("'") + key + "' must be an integer");
                }
                return rJson.at(key).get<int>();
            };

            PopCompositionConfig_t config;
            config.bureaucracyLimitFormula = requireNonEmptyString("bureaucracy_limit_formula");
            config.bureaucracyDroneFormula = requireNonEmptyString("bureaucracy_drone_formula");
            config.sizeDroneFormula = requireNonEmptyString("size_drone_formula");
            config.occupationDroneFormula = requireNonEmptyString("occupation_drone_formula");
            config.droneTypeId = requireNonEmptyString("drone_type");
            config.talentTypeId = requireNonEmptyString("talent_type");
            config.assimilationDrones = requirePositiveInt("assimilation_drones");
            config.assimilationDecayTurns = requirePositiveInt("assimilation_decay_turns");
            config.riotThreshold = requireInt("riot_threshold");
            config.goldenAgeThreshold = requireInt("golden_age_threshold");
            config.effects = EffectConfigParser::ParseEffects(
                rJson, EffectSourceKind_t::PopComposition, "pop_composition");
            if (rJson.contains("golden_age_effects"))
            {
                config.goldenAgeEffects = ParseEffectArray_(rJson.at("golden_age_effects"),
                                                            "pop_composition.golden_age_effects",
                                                            fail);
            }
            if (rJson.contains("riot_tiers"))
            {
                const nlohmann::json& rTiersJson = rJson.at("riot_tiers");
                if (!rTiersJson.is_array())
                {
                    fail("'riot_tiers' must be a JSON array");
                }
                for (const auto& rTierJson : rTiersJson)
                {
                    if (!rTierJson.is_object())
                    {
                        fail("riot_tiers entries must be objects");
                    }
                    RiotTier_t tier;
                    if (!rTierJson.contains("min_turns")
                        || !rTierJson.at("min_turns").is_number_integer())
                    {
                        fail("riot_tiers entry missing integer 'min_turns'");
                    }
                    tier.minTurns = rTierJson.at("min_turns").get<int>();
                    if (tier.minTurns < 1)
                    {
                        fail("riot_tiers.min_turns must be >= 1, got "
                             + std::to_string(tier.minTurns));
                    }
                    if (rTierJson.contains("effects"))
                    {
                        tier.effects = ParseEffectArray_(rTierJson.at("effects"),
                                                         "pop_composition.riot_tiers.effects",
                                                         fail);
                    }
                    if (rTierJson.contains("on_enter_effects"))
                    {
                        tier.onEnterEffects = ParseEffectArray_(
                            rTierJson.at("on_enter_effects"),
                            "pop_composition.riot_tiers.on_enter_effects", fail);
                    }
                    config.riotTiers.push_back(std::move(tier));
                }
            }

            // Required, and every field within it: these are the rebellion's tuning numbers,
            // so a missing key must fail at load rather than silently resolve to whatever a
            // C++ member initializer happened to say.
            if (!rJson.contains("rebel_selection") || !rJson.at("rebel_selection").is_object())
            {
                fail("'rebel_selection' must be an object");
            }
            const nlohmann::json& rRebel = rJson.at("rebel_selection");
            static const std::vector<std::string> knownRebelKeys = {
                "distance_mode", "fade_radius", "distance_weight_per_tile",
                "base_join_weight", "missing_hq_distance",
            };
            for (const auto& [rKey, rUnused] : rRebel.items())
            {
                if (std::find(knownRebelKeys.begin(), knownRebelKeys.end(), rKey)
                    == knownRebelKeys.end())
                {
                    fail("unknown key 'rebel_selection." + rKey + "'");
                }
            }
            const auto requireRebelInt = [&](const char* key, int minimum) {
                if (!rRebel.contains(key) || !rRebel.at(key).is_number_integer())
                {
                    fail(std::string("'rebel_selection.") + key + "' must be an integer");
                }
                const int value = rRebel.at(key).get<int>();
                if (value < minimum)
                {
                    fail(std::string("'rebel_selection.") + key + "' must be >= "
                         + std::to_string(minimum) + ", got " + std::to_string(value));
                }
                return value;
            };
            if (!rRebel.contains("distance_mode") || !rRebel.at("distance_mode").is_string())
            {
                fail("'rebel_selection.distance_mode' must be a string");
            }
            const std::string modeWire = rRebel.at("distance_mode").get<std::string>();
            const std::optional<RebelDistanceMode_t> mode = ParseRebelDistanceMode(modeWire);
            if (!mode)
            {
                fail("rebel_selection.distance_mode must be none, nearest_base, hq_distance, "
                     "or nearby_bases, got '" + modeWire + "'");
            }
            config.rebelSelection.distanceMode = *mode;
            config.rebelSelection.fadeRadius = requireRebelInt("fade_radius", 0);
            config.rebelSelection.distanceWeightPerTile =
                requireRebelInt("distance_weight_per_tile", 0);
            config.rebelSelection.baseJoinWeight = requireRebelInt("base_join_weight", 0);
            config.rebelSelection.missingHqDistance = requireRebelInt("missing_hq_distance", 0);
            return config;
        });
}

} // namespace ac
