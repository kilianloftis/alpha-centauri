#pragma once

#include "game/effects/BonusEffect.h"

#include <vector>

namespace ac
{

// Standing Planetary Council rules (propose intervals and Planetary Governor benefits).
// Loaded from config/council/rules.json beside the proposal list.
struct CouncilRulesConfig_t
{
    int governorProposeIntervalYears = 10;
    int memberProposeIntervalYears = 20;
    // Effects granted when a faction holds the governorship. Continuous FactionGlobal
    // entries feed CouncilEffects; Instantaneous entries (e.g. Infiltration) are applied
    // by CouncilOutcomeApplier::ApplyGovernor at election time.
    std::vector<EffectConfig_t> governorEffects;
};

} // namespace ac
