#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <vector>

namespace ac
{

// Standing Planetary Council rules (propose intervals and Planetary Governor benefits).
// Loaded from config/council/rules.json beside the proposal list.
struct CouncilRulesConfig_t
{
    int governorProposeIntervalYears = 10;
    int memberProposeIntervalYears = 20;
    // Standing effects held for as long as a faction holds the governorship (FactionGlobal;
    // fed to CouncilEffects).
    std::vector<EffectConfig_t> governorEffects;
    // One-shot perks applied once, when a governor takes office — e.g. a SetInfiltration that
    // outlives the term. Applied by CouncilOutcomeApplier::ApplyGovernor.
    std::vector<TriggeredEffectConfig_t> onElectedEffects;
};

} // namespace ac
