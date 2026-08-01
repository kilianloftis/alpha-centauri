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
    // When true, the Planetary Governor gains infiltration over every other council member.
    bool governorInfiltratesMembers = true;
    // Continuous FactionGlobal effects granted while a faction holds the governorship.
    std::vector<EffectConfig_t> governorEffects;
};

} // namespace ac
