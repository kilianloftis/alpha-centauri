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

namespace
{

// One side of the fight, with the context and morale bonus its stats resolve under.
struct Fighter_t
{
    const Unit& rUnit;
    EffectContext_t ctx;
    double moralePercent;
};

Fighter_t MakeFighter_(const Unit& rUnit, const Unit& rAttacker, const Unit& rDefender,
                       CombatRole_t role, const MoraleCalculator& rMorale)
{
    EffectContext_t ctx{&rDefender.GetTile(), role};
    ctx.pAttacker = &rAttacker;
    ctx.pDefender = &rDefender;
    const int level = CombatMoraleLevel(rUnit, ctx, rMorale);
    return Fighter_t{rUnit, ctx, rMorale.CombatAddPercent(level)};
}

double PsiRating_(const Fighter_t& rFighter, StatId_t stat)
{
    return ResolveCombatUnitMultiplicativeStat(rFighter.rUnit, stat, 1.0, rFighter.ctx,
                                               rFighter.moralePercent);
}

double ConventionalRating_(const Fighter_t& rFighter, StatId_t stat)
{
    const int rating =
        ResolveCombatUnitStat(rFighter.rUnit, stat, rFighter.ctx, rFighter.moralePercent);
    return static_cast<double>(rating);
}

// An attacker with less than one move point left strikes at that fraction of its rating.
int PartialMoveAttack_(double attackRating, int remainingFragments)
{
    const double perPoint = static_cast<double>(MovementConstants_t::k_moveFragmentsPerPoint);
    const double scaled =
        attackRating * static_cast<double>(remainingFragments) / perPoint * k_CombatStrengthScale;
    return static_cast<int>(std::lround(scaled));
}

} // namespace

CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement)
{
    // TODO(difficulty): apply combat handicap from rules.combat_handicap /
    // combat_handicap_natives_only once magnitude is known (do not invent percents).
    CombatStrength_t strength;
    const Fighter_t attacker =
        MakeFighter_(rAttacker, rAttacker, rDefender, CombatRole_t::Attacker, rMorale);
    const Fighter_t defender =
        MakeFighter_(rDefender, rAttacker, rDefender, CombatRole_t::Defender, rMorale);
    const StatId_t defenderStat = engagement == CombatEngagement_t::ArtilleryDuel
                                      ? StatId_t::Attack
                                      : StatId_t::Defense;
    strength.bPsiCombat = ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat)
                          || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat);
    const double attackRating = strength.bPsiCombat
                                    ? PsiRating_(attacker, StatId_t::Attack)
                                    : ConventionalRating_(attacker, StatId_t::Attack);
    const double defenseRating = strength.bPsiCombat
                                     ? PsiRating_(defender, defenderStat)
                                     : ConventionalRating_(defender, defenderStat);
    const double tileDefenseMult = rTileEffects.ResolveTileDefenseMultiplier(
        rDefender.GetTile(), rDefender.GetFaction().GetFactionId());

    strength.attackStrength = static_cast<int>(std::lround(attackRating * k_CombatStrengthScale));
    strength.defenseStrength =
        static_cast<int>(std::lround(defenseRating * tileDefenseMult * k_CombatStrengthScale));
    const int remainingFragments = rAttacker.GetMoveFragmentsRemaining();
    if (remainingFragments < MovementConstants_t::k_moveFragmentsPerPoint)
    {
        strength.attackStrength = PartialMoveAttack_(attackRating, remainingFragments);
    }
    return strength;
}

} // namespace ac
