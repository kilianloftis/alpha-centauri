#pragma once

#include "game/combat/MoraleConfig.h"

#include <string>

namespace ac
{

class LuaRuntime;

enum class MoraleLifecycle_t
{
    Conventional,
    Native,
};

// Morale / XP rank formulas over the game-wide MoraleConfig_t. Combat ratings scale by
// CombatMultiplier; named rows keep promotion eligibility/success and planet_pearls.
class MoraleCalculator
{
public:
    MoraleCalculator(const MoraleConfig_t& rConfig, LuaRuntime& rLua);

    const MoraleConfig_t& GetConfig() const;

    int StartingIntrinsicXp() const;

    // Clamped to the configured levels. Combat-role floor is applied to the combined
    // defense level in CombatStrength, not here.
    int MoraleLevel(int xp, int liveBonus) const;

    // 1 + (combatLevel - baselineIndex) * percentPerLevel / 100. Unbounded: attack may sit
    // outside the named rows.
    double CombatMultiplier(int combatLevel) const;

    const std::string& DisplayName(int level, MoraleLifecycle_t lifecycle) const;

    // Root promotion_eligibility_formula Lua from the survivor's point of view.
    double PromotionEligibilitySeed(int ourStrength, int theirStrength) const;

private:
    const MoraleConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
