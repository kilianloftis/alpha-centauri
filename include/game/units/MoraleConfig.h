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
    // Combat Attack/Defense AddPercent and promotion_chance stack modifiers for this
    // intrinsic level. promotion_chance seed comes from MoraleConfig_t::promotionSeedFormula.
    std::vector<EffectConfig_t> effects;
};

struct MoraleConfig_t
{
    int baseIntrinsic = 1;
    int probeBaseIntrinsic = 2;
    int defenseFloorIndex = 1;
    // Lua seed for promotion_chance ApplyModifierStack. Vars: attack_strength, defense_strength
    // (final post-combat weapon and armor strengths from the kill).
    std::string promotionSeedFormula;
    std::vector<MoraleLevel_t> levels;

    int MinLevel() const;
    int MaxLevel() const;
    const MoraleLevel_t* FindLevel(int index) const;
};

} // namespace ac
