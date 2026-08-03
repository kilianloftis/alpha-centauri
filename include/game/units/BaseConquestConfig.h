#pragma once

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
    int lastDefenderPopLoss = 1;
    int capturePopLoss = 1;
    // Uniform count in [min, floor(eligible * maxPercent / 100)], inclusive.
    int captureFacilitiesDestroyedMin = 0;
    int captureFacilitiesDestroyedMaxPercent = 50;
    EscapeColonyPodConfig_t escapeColonyPod;
};

} // namespace ac
