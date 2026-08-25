#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/units/MoraleConfig.h"

#include <random>
#include <span>
#include <string>

namespace ac
{

class LuaRuntime;
class Unit;

// Morale / XP rank rules backed by game-wide MoraleConfig_t (levels, promotion).
// Holds a non-owning config reference — same pattern as TechCostCalculator.
// Combat Attack/Defense bonuses live as effects on each MoraleLevel_t and are folded
// in by ResolveCombatUnitStat — this class only picks the effective level.
class MoraleCalculator
{
public:
    MoraleCalculator(const MoraleConfig_t& rConfig, LuaRuntime& rLua);

    const MoraleConfig_t& GetConfig() const;

    int BaseIntrinsicXp(const Unit& rUnit) const;

    // Live morale level for combat (SE bonus, creche soften, riot, defense floor).
    int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx) const;

    // Effects declared on the unit's effective morale level (empty if level missing).
    // Includes promotion_chance modifiers; combat resolve ignores stats it does not query.
    std::span<const EffectConfig_t> EffectiveLevelEffects(const Unit& rUnit,
                                                          const EffectContext_t& rCtx) const;

    const std::string& DisplayName(const Unit& rUnit) const;
    const std::string& DisplayName(int level, bool bNativeLifecycle) const;

    // Promote survivor one intrinsic XP on success. Skips ProbeTeam. Caps at max level.
    // Promotion chance uses intrinsic XP level effects (not SE-shifted effective level).
    bool TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength,
                    std::mt19937& rRng) const;

private:
    const MoraleConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

// Stack intrinsic-level promotion_chance modifiers on a Lua-computed seed (MoraleCalculator only).
double ResolvePromotionChanceFromLevel(double seed,
                                       std::span<const EffectConfig_t> levelEffects);

} // namespace ac
