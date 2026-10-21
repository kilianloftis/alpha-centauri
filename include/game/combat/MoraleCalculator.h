#pragma once

#include "game/combat/MoraleConfig.h"

#include <span>
#include <string>

namespace ac
{

class LuaRuntime;

enum class MoraleLifecycle_t
{
    Conventional,
    Native,
};

// Morale / XP rank formulas over the game-wide MoraleConfig_t. Combat Attack/Defense
// scale is CombatAddPercent; named rows keep promotion_chance and planet_pearls.
class MoraleCalculator
{
public:
    MoraleCalculator(const MoraleConfig_t& rConfig, LuaRuntime& rLua);

    const MoraleConfig_t& GetConfig() const;

    int StartingIntrinsicXp() const;

    // Clamped to the configured levels. Combat-role floor is applied to the combined
    // defense level in CombatStrength, not here.
    int MoraleLevel(int xp, int liveBonus) const;

    // Includes promotion_chance modifiers; combat resolve ignores stats it does not query.
    std::span<const EffectConfig_t> LevelEffects(int level) const;

    // (combatLevel - baselineIndex) * percentPerLevel. Unbounded: attack may sit
    // outside the named rows.
    double CombatAddPercent(int combatLevel) const;

    const std::string& DisplayName(int level, MoraleLifecycle_t lifecycle) const;

    // Uses the intrinsic level's effects, not the SE-shifted effective level.
    double PromotionChance(int xp, int attackStrength, int defenseStrength) const;

private:
    const MoraleConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

// Stack intrinsic-level promotion_chance modifiers on a Lua-computed seed (MoraleCalculator only).
double ResolvePromotionChanceFromLevel(double seed,
                                       std::span<const EffectConfig_t> levelEffects);

} // namespace ac
