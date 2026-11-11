#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

struct MoraleLevel_t
{
    int index = 0;
    std::string conventional;
    std::string native;
    // Level modifiers for promotion_* and planet_pearls when they differ from identity
    // (e.g. Green MinClamp eligibility, Hardened ×0.5 success). Combat Attack/Defense
    // scale is MoraleConfig_t::percentPerLevel.
    std::vector<EffectConfig_t> effects;
};

struct MoraleConfig_t
{
    int startingIntrinsic = 1;
    int defenseFloorIndex = 1;
    // Combat AddPercent = (combatLevel - baselineIndex) * percentPerLevel.
    double percentPerLevel = 0.0;
    int baselineIndex = 0;
    // Lua seed for StatId PromotionEligibilityChance. Vars: our_strength, their_strength.
    std::string promotionEligibilityFormula;
    std::vector<MoraleLevel_t> levels;

    int MinLevel() const;
    int MaxLevel() const;
    const MoraleLevel_t* FindLevel(int index) const;
};

} // namespace ac
