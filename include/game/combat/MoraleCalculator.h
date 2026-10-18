#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/combat/MoraleConfig.h"

#include <span>
#include <string>

namespace ac
{

class LuaRuntime;

// Morale / XP rank formulas over the game-wide MoraleConfig_t. It only picks the level:
// ResolveCombatUnitStat folds in each MoraleLevel_t's Attack/Defense effects.
class MoraleCalculator
{
public:
    MoraleCalculator(const MoraleConfig_t& rConfig, LuaRuntime& rLua);

    const MoraleConfig_t& GetConfig() const;

    int IntrinsicXp(bool bProbeTeam) const;

    // Clamped to the configured levels, with the defense floor for a defender.
    int MoraleLevel(int xp, int liveBonus, CombatRole_t role) const;

    // Includes promotion_chance modifiers; combat resolve ignores stats it does not query.
    std::span<const EffectConfig_t> LevelEffects(int level) const;

    const std::string& DisplayName(int level, bool bNativeLifecycle) const;

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
