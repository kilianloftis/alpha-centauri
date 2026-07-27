#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/units/MoraleConfig.h"

#include <random>
#include <string>

namespace ac
{

class Unit;

// Morale / XP rank rules backed by game-wide MoraleConfig_t (levels, combat %, promotion).
// Holds a non-owning config reference — same pattern as TechCostCalculator.
class MoraleCalculator
{
public:
    explicit MoraleCalculator(const MoraleConfig_t& rConfig);

    const MoraleConfig_t& GetConfig() const;

    int BaseIntrinsicXp(const Unit& rUnit) const;

    // Live morale level for combat (SE bonus, creche soften, riot, defense floor).
    int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx) const;

    // combat_bonus_percent for the unit's effective level (fed into AddPercent stack).
    double CombatMoraleAddPercent(const Unit& rUnit, const EffectContext_t& rCtx) const;

    const std::string& DisplayName(const Unit& rUnit) const;
    const std::string& DisplayName(int level, bool bNativeLifecycle) const;

    // Promote survivor one intrinsic XP on success. Skips ProbeTeam. Caps at max level.
    bool TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength,
                    std::mt19937& rRng) const;

    // Resolves Attack/Defense for combat: live unit effects + synthetic morale AddPercent.
    int ResolveCombatStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx) const;
    double ResolveCombatMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                           const EffectContext_t& rCtx) const;

private:
    const MoraleConfig_t& m_rConfig;
};

} // namespace ac
