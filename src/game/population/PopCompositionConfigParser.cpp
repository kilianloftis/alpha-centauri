#include "game/population/PopCompositionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
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

// Reads one JSON object, naming the config file and the key's path in every error.
class CompositionReader_
{
public:
    CompositionReader_(const nlohmann::json& rJson, const std::string& rConfigPath,
                       std::string keyPrefix)
        : m_rJson(rJson)
        , m_context("Pop composition config '" + rConfigPath + "'")
        , m_keyPrefix(std::move(keyPrefix))
    {
    }

    [[noreturn]] void Fail(const std::string& rMessage) const
    {
        throw std::runtime_error(m_context + ": " + rMessage);
    }

    void RejectUnknownKeys(const std::vector<std::string>& rKnown) const
    {
        for (const auto& [rKey, rUnused] : m_rJson.items())
        {
            if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
            {
                Fail("unknown key '" + m_keyPrefix + rKey + "'");
            }
        }
    }

    std::string NonEmptyString(const char* pKey) const
    {
        if (!m_rJson.contains(pKey) || !m_rJson.at(pKey).is_string()
            || m_rJson.at(pKey).get<std::string>().empty())
        {
            Fail(Quoted_(pKey) + " must be a non-empty string");
        }
        return m_rJson.at(pKey).get<std::string>();
    }

    int Int(const char* pKey) const
    {
        if (!m_rJson.contains(pKey) || !m_rJson.at(pKey).is_number_integer())
        {
            Fail(Quoted_(pKey) + " must be an integer");
        }
        return m_rJson.at(pKey).get<int>();
    }

    int PositiveInt(const char* pKey) const
    {
        if (!m_rJson.contains(pKey) || !m_rJson.at(pKey).is_number_integer())
        {
            Fail(Quoted_(pKey) + " must be a positive integer");
        }
        const int value = m_rJson.at(pKey).get<int>();
        if (value <= 0)
        {
            Fail(Quoted_(pKey) + " must be > 0, got " + std::to_string(value));
        }
        return value;
    }

    int IntAtLeast(const char* pKey, int minimum) const
    {
        const int value = Int(pKey);
        if (value < minimum)
        {
            Fail(Quoted_(pKey) + " must be >= " + std::to_string(minimum) + ", got "
                 + std::to_string(value));
        }
        return value;
    }

private:
    std::string Quoted_(const char* pKey) const
    {
        return "'" + m_keyPrefix + pKey + "'";
    }

    const nlohmann::json& m_rJson;
    std::string m_context;
    std::string m_keyPrefix;
};

std::vector<EffectConfig_t> ParseEffectArray_(const nlohmann::json& rArray,
                                              const std::string& rPath,
                                              const CompositionReader_& rReader)
{
    if (!rArray.is_array())
    {
        rReader.Fail("'" + rPath + "' must be a JSON array");
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

RiotTier_t ParseRiotTier_(const nlohmann::json& rTierJson, const CompositionReader_& rReader)
{
    if (!rTierJson.is_object())
    {
        rReader.Fail("riot_tiers entries must be objects");
    }
    RiotTier_t tier;
    if (!rTierJson.contains("min_turns") || !rTierJson.at("min_turns").is_number_integer())
    {
        rReader.Fail("riot_tiers entry missing integer 'min_turns'");
    }
    tier.minTurns = rTierJson.at("min_turns").get<int>();
    if (tier.minTurns < 1)
    {
        rReader.Fail("riot_tiers.min_turns must be >= 1, got " + std::to_string(tier.minTurns));
    }
    if (rTierJson.contains("effects"))
    {
        tier.effects = ParseEffectArray_(rTierJson.at("effects"),
                                         "pop_composition.riot_tiers.effects", rReader);
    }
    tier.onEnterEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rTierJson, "on_enter_effects", "pop_composition.riot_tiers");
    return tier;
}

std::vector<RiotTier_t> ParseRiotTiers_(const nlohmann::json& rTiersJson,
                                        const CompositionReader_& rReader)
{
    if (!rTiersJson.is_array())
    {
        rReader.Fail("'riot_tiers' must be a JSON array");
    }
    std::vector<RiotTier_t> tiers;
    for (const auto& rTierJson : rTiersJson)
    {
        tiers.push_back(ParseRiotTier_(rTierJson, rReader));
    }
    return tiers;
}

// Required, every field: these are the rebellion's tuning numbers.
RebelSelectionConfig_t ParseRebelSelection_(const nlohmann::json& rJson,
                                            const std::string& rConfigPath,
                                            const CompositionReader_& rRootReader)
{
    if (!rJson.contains("rebel_selection") || !rJson.at("rebel_selection").is_object())
    {
        rRootReader.Fail("'rebel_selection' must be an object");
    }
    const nlohmann::json& rRebel = rJson.at("rebel_selection");
    const CompositionReader_ reader(rRebel, rConfigPath, "rebel_selection.");
    reader.RejectUnknownKeys({"distance_mode", "fade_radius", "distance_weight_per_tile",
                              "base_join_weight", "missing_hq_distance"});
    if (!rRebel.contains("distance_mode") || !rRebel.at("distance_mode").is_string())
    {
        reader.Fail("'rebel_selection.distance_mode' must be a string");
    }
    const std::string modeWire = rRebel.at("distance_mode").get<std::string>();
    const std::optional<RebelDistanceMode_t> mode = ParseRebelDistanceMode(modeWire);
    if (!mode)
    {
        reader.Fail("rebel_selection.distance_mode must be none, nearest_base, hq_distance, "
                    "or nearby_bases, got '" + modeWire + "'");
    }
    RebelSelectionConfig_t selection;
    selection.distanceMode = *mode;
    selection.fadeRadius = reader.IntAtLeast("fade_radius", 0);
    selection.distanceWeightPerTile = reader.IntAtLeast("distance_weight_per_tile", 0);
    selection.baseJoinWeight = reader.IntAtLeast("base_join_weight", 0);
    selection.missingHqDistance = reader.IntAtLeast("missing_hq_distance", 0);
    return selection;
}

PopCompositionConfig_t ParseComposition_(const nlohmann::json& rJson,
                                         const std::string& rConfigPath)
{
    const CompositionReader_ reader(rJson, rConfigPath, "");
    reader.RejectUnknownKeys({
        "bureaucracy_limit_formula", "bureaucracy_drone_formula",
        "size_drone_formula", "occupation_drone_formula",
        "drone_type", "talent_type",
        "assimilation_drones", "assimilation_decay_turns",
        "riot_threshold", "golden_age_threshold",
        "effects", "golden_age_effects", "riot_tiers", "rebel_selection",
    });
    PopCompositionConfig_t config;
    config.bureaucracyLimitFormula = reader.NonEmptyString("bureaucracy_limit_formula");
    config.bureaucracyDroneFormula = reader.NonEmptyString("bureaucracy_drone_formula");
    config.sizeDroneFormula = reader.NonEmptyString("size_drone_formula");
    config.occupationDroneFormula = reader.NonEmptyString("occupation_drone_formula");
    config.droneTypeId = reader.NonEmptyString("drone_type");
    config.talentTypeId = reader.NonEmptyString("talent_type");
    config.assimilationDrones = reader.PositiveInt("assimilation_drones");
    config.assimilationDecayTurns = reader.PositiveInt("assimilation_decay_turns");
    config.riotThreshold = reader.Int("riot_threshold");
    config.goldenAgeThreshold = reader.Int("golden_age_threshold");
    config.effects = EffectConfigParser::ParseEffects(
        rJson, EffectSourceKind_t::PopComposition, "pop_composition");
    if (rJson.contains("golden_age_effects"))
    {
        config.goldenAgeEffects = ParseEffectArray_(
            rJson.at("golden_age_effects"), "pop_composition.golden_age_effects", reader);
    }
    if (rJson.contains("riot_tiers"))
    {
        config.riotTiers = ParseRiotTiers_(rJson.at("riot_tiers"), reader);
    }
    config.rebelSelection = ParseRebelSelection_(rJson, rConfigPath, reader);
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
        configPath, "pop composition", [&configPath](const nlohmann::json& rJson) {
            return ParseComposition_(rJson, configPath);
        });
}

} // namespace ac
