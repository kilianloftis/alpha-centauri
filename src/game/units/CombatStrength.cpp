#include "game/units/CombatStrength.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/Faction.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Unit.h"

#include <cmath>

namespace ac
{

CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale)
{
    // TODO(difficulty): apply combat handicap from rules.combat_handicap /
    // combat_handicap_natives_only once magnitude is known (do not invent percents).
    CombatStrength_t strength;

    EffectContext_t attackCtx{&rDefender.GetTile(), CombatRole_t::Attacker};
    EffectContext_t defenseCtx{&rDefender.GetTile(), CombatRole_t::Defender};
    attackCtx.pAttacker = &rAttacker;
    defenseCtx.pAttacker = &rAttacker;
    attackCtx.pDefender = &rDefender;
    defenseCtx.pDefender = &rDefender;
    const double tileDefenseMult = rTileEffects.ResolveTileDefenseMultiplier(
        rDefender.GetTile(), rDefender.GetFaction().GetFactionId());

    strength.bPsiCombat = ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat)
                          || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat);
    double attackRating = 0.0;
    if (strength.bPsiCombat)
    {
        attackRating = ResolveCombatUnitMultiplicativeStat(
            rAttacker, StatId_t::Attack, 1.0, attackCtx,
            rMorale.EffectiveLevelEffects(rAttacker, attackCtx));
        const double defenseRating = ResolveCombatUnitMultiplicativeStat(
            rDefender, StatId_t::Defense, 1.0, defenseCtx,
            rMorale.EffectiveLevelEffects(rDefender, defenseCtx));
        strength.attackStrength = static_cast<int>(
            std::lround(attackRating * k_CombatStrengthScale));
        strength.defenseStrength = static_cast<int>(std::lround(
            defenseRating * tileDefenseMult * k_CombatStrengthScale));
    }
    else
    {
        const int conventionalAttack = ResolveCombatUnitStat(
            rAttacker, StatId_t::Attack, attackCtx,
            rMorale.EffectiveLevelEffects(rAttacker, attackCtx));
        const int defenseRating = ResolveCombatUnitStat(
            rDefender, StatId_t::Defense, defenseCtx,
            rMorale.EffectiveLevelEffects(rDefender, defenseCtx));
        attackRating = static_cast<double>(conventionalAttack);
        strength.attackStrength = conventionalAttack * k_CombatStrengthScale;
        strength.defenseStrength = static_cast<int>(std::lround(
            defenseRating * tileDefenseMult * k_CombatStrengthScale));
    }

    const int remainingFragments = rAttacker.GetMoveFragmentsRemaining();
    if (remainingFragments < MovementConstants_t::k_moveFragmentsPerPoint)
    {
        strength.attackStrength = static_cast<int>(std::lround(
            attackRating * static_cast<double>(remainingFragments)
            / static_cast<double>(MovementConstants_t::k_moveFragmentsPerPoint)
            * k_CombatStrengthScale));
    }
    return strength;
}

} // namespace ac
