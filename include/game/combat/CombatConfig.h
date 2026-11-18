#pragma once

#include "game/effects/EffectConfig.h"

#include <vector>

namespace ac
{

// Combat tunables and standing combat rules. Loaded from config/combat.json.
struct CombatConfig_t
{
    // Smallest population the nearest base needs to become a boarded unit's home base.
    int boardingHomeBaseMinPop = 0;
    // Standing WorldGlobal combat rules, such as the psi values.
    std::vector<EffectConfig_t> effects;
};

} // namespace ac
