#pragma once

#include "game/effects/EffectConfig.h"
#include <string>
#include <vector>

namespace ac
{

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
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    PopCompositionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
