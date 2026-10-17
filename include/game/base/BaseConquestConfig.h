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

struct BaseConquestConfig_t
{
    // Continuous effects injected into every faction's pool: the LastDefenderPopLoss,
    // CapturePopLoss, CaptureFacilitiesDestroyedMin, CaptureFacilitiesDestroyedMaxPercent
    // and ConqueredDroneCap baselines, each an Add. Every numeric conquest tunable is a
    // modifiable stat rather than a hard-coded seed, so a mod can shift any of them without
    // touching C++.
    std::vector<EffectConfig_t> effects;
    EscapeColonyPodConfig_t escapeColonyPod;
};

} // namespace ac
