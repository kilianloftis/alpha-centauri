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
    // promotion_chance and planet_pearls stack modifiers for this intrinsic level.
    // Combat Attack/Defense scale is MoraleConfig_t::percentPerLevel, not a row effect.
    std::vector<EffectConfig_t> effects;
};

struct MoraleConfig_t
{
    int startingIntrinsic = 1;
    int defenseFloorIndex = 1;
    // Combat AddPercent = (combatLevel - baselineIndex) * percentPerLevel.
    double percentPerLevel = 0.0;
    int baselineIndex = 0;
    // Lua seed for promotion_chance ApplyModifierStack. Vars: attack_strength, defense_strength
    // (final post-combat weapon and armor strengths from the kill).
    std::string promotionSeedFormula;
    std::vector<MoraleLevel_t> levels;

    int MinLevel() const;
    int MaxLevel() const;
    const MoraleLevel_t* FindLevel(int index) const;
};

} // namespace ac
