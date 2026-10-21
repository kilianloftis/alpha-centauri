#include "game/combat/CombatStrength.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/movement/MovementConstants.h"
#include "game/units/Unit.h"

#include <cmath>

namespace ac
{

CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement)
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

    const StatId_t defenderStat = engagement == CombatEngagement_t::ArtilleryDuel
                                      ? StatId_t::Attack
                                      : StatId_t::Defense;
    strength.bPsiCombat = ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat)
                          || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat);
    const int attackLevel = CombatMoraleLevel(rAttacker, attackCtx, rMorale);
    const int defenseLevel = CombatMoraleLevel(rDefender, defenseCtx, rMorale);
    const double attackPercent = rMorale.CombatAddPercent(attackLevel);
    const double defensePercent = rMorale.CombatAddPercent(defenseLevel);

    double attackRating = 0.0;
    if (strength.bPsiCombat)
    {
        attackRating = ResolveCombatUnitMultiplicativeStat(
            rAttacker, StatId_t::Attack, 1.0, attackCtx, attackPercent);
        const double defenseRating = ResolveCombatUnitMultiplicativeStat(
            rDefender, defenderStat, 1.0, defenseCtx, defensePercent);
        strength.attackStrength = static_cast<int>(
            std::lround(attackRating * k_CombatStrengthScale));
        strength.defenseStrength = static_cast<int>(std::lround(
            defenseRating * tileDefenseMult * k_CombatStrengthScale));
    }
    else
    {
        const int conventionalAttack = ResolveCombatUnitStat(
            rAttacker, StatId_t::Attack, attackCtx, attackPercent);
        const int defenseRating = ResolveCombatUnitStat(
            rDefender, defenderStat, defenseCtx, defensePercent);
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
