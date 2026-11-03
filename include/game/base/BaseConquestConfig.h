#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

// Tunables for base conquest / native raids. Loaded from config/base_conquest.json.
struct EscapeColonyPodConfig_t
{
    // Component ids assembled into a FoundBase design for fleeing colonists
    // (same shape as test MakeUnit component lists). Empty = skip pod spawns.
    std::vector<std::string> componentIds;
};

struct NativeRaidConfig_t
{
    // Chance to destroy a facility; otherwise the raid takes popLoss. A living base
    // always has population — size 0 razes it.
    int facilityChancePercent = 0;
    int popLoss = 0;
};

// capture_base's ESCAPE (terranx-map.md "Base transfer and Headquarters"): the owner of a
// captured Headquarters pays cost to move it to the base with the lowest score,
// (scoreSeed + same-region distances + otherRegionDistanceFactor x other-region distances)
// x scoreScale / (region bases + populationFactor x population), to its other bases.
struct HeadquartersEscapeConfig_t
{
    int cost = 0;
    int scoreSeed = 0;
    int scoreScale = 0;
    int otherRegionDistanceFactor = 0;
    int populationFactor = 0;
};

struct BaseConquestConfig_t
{
    // Continuous effects in every faction's pool: the Add baselines for each conquest stat, so a
    // mod can move any of them.
    std::vector<EffectConfig_t> effects;
    EscapeColonyPodConfig_t escapeColonyPod;
    NativeRaidConfig_t nativeRaid;
    HeadquartersEscapeConfig_t headquartersEscape;
};

} // namespace ac
