#pragma once

#include "game/effects/EffectConfig.h"
#include <optional>
#include <string>
#include <vector>

namespace ac
{

struct RiotTier_t
{
    // Minimum consecutive committed riot turns for this tier (inclusive).
    int minTurns = 1;
    // Continuous effects while this tier is the active (highest matching) tier. ThisBase
    // entries resolve through BaseEffectsCache; FactionUnits entries reach units homed at the
    // rioting base through the faction pool. See BaseMoodEffects.
    std::vector<EffectConfig_t> effects;
    // Instantaneous effects dispatched each Mood commit while at this tier.
    std::vector<EffectConfig_t> onEnterEffects;
};

// How physical distance weights a faction's chance of receiving a rebelling base.
enum class RebelDistanceMode_t
{
    // Distance is ignored; only RebelJoinWeight decides.
    None,
    // Bonus fades with the distance to the candidate's single closest base.
    NearestBase,
    // Bonus fades with the distance to the candidate's headquarters.
    HqDistance,
    // Every candidate base within fadeRadius contributes; a cluster outweighs a lone base.
    NearbyBases,
};

// Snake_case JSON wire form differs from the enumerator names — one explicit map, here next
// to the enum rather than duplicated at the parser.
inline const char* RebelDistanceModeWireName(RebelDistanceMode_t mode)
{
    switch (mode)
    {
        case RebelDistanceMode_t::None:        return "none";
        case RebelDistanceMode_t::NearestBase: return "nearest_base";
        case RebelDistanceMode_t::HqDistance:  return "hq_distance";
        case RebelDistanceMode_t::NearbyBases: return "nearby_bases";
    }
    return "none";
}

// Empty when the wire form matches no mode; the caller reports the config error.
std::optional<RebelDistanceMode_t> ParseRebelDistanceMode(const std::string& rMode);

struct RebelSelectionConfig_t
{
    RebelDistanceMode_t distanceMode = RebelDistanceMode_t::None;
    // Distance at which the bonus reaches zero. Named for the fade, not for one mode: every
    // distance mode measures its bonus against this radius.
    int fadeRadius = 0;
    int distanceWeightPerTile = 0;
    // Resolve seed for the RebelJoinWeight stat, so factions declaring no modifier still
    // participate. A seed of 0 means only factions with an explicit modifier can receive.
    int baseJoinWeight = 0;
    // Substituted when a candidate faction has no headquarters (HqDistance mode).
    int missingHqDistance = 0;
};

struct PopCompositionConfig_t
{
    std::string bureaucracyLimitFormula;
    // One formula per drone source, rather than a single expression summing all of them. Each
    // is evaluated on its own inputs and contributes to the Drones seed, which makes a term
    // testable in isolation and lets a mod replace one source without restating the others.
    std::string bureaucracyDroneFormula;
    std::string sizeDroneFormula;
    std::string occupationDroneFormula;
    std::string droneTypeId;
    std::string talentTypeId;
    // Peak extra drones on the capture turn, decaying by 1 every assimilationDecayTurns.
    // Fresh-capture duration is the product (shipping 5 × 10 = 50).
    int assimilationDrones = 0;
    int assimilationDecayTurns = 0;
    // Game rules for mood checks (docs/game-rules-decisions.md §9). Not effect stats — facilities
    // change mood via composition (Drones, Talents, psych), not by moving these thresholds.
    int riotThreshold = 1;
    int goldenAgeThreshold = 0;
    // Drone-pressure clamps injected into every faction pool (MinClamp 0, MaxClamp BaseSize).
    std::vector<EffectConfig_t> effects;
    // While IsInGoldenAge(), appended per base in BaseEffectsCache (not faction-wide).
    std::vector<EffectConfig_t> goldenAgeEffects;
    // Escalation tiers keyed by consecutive committed riot turns. Each entry is self-contained.
    std::vector<RiotTier_t> riotTiers;
    // Method of choosing faction to which the base should defect
    RebelSelectionConfig_t rebelSelection;
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    PopCompositionConfig_t ParseConfig(const std::string& configPath);
};

// Highest tier where consecutiveTurns >= minTurns, or nullptr.
const RiotTier_t* FindActiveRiotTier(const PopCompositionConfig_t& rConfig, int consecutiveTurns);

} // namespace ac
