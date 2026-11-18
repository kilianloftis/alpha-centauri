#include "game/combat/CombatRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/movement/MovementConstants.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace ac
{

namespace
{

bool IsPsiCombat_(const Unit& rAttacker, const Unit& rDefender)
{
    return ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat)
           || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat);
}

int RoundDamage_(const Unit& rReceiver, bool bPsiCombat)
{
    if (!bPsiCombat)
    {
        return k_CombatRoundDamage;
    }
    return std::max(1, ResolveStat(rReceiver, StatId_t::PsiDamage));
}

// The value stats a fight rates each side on. Both kinds share the Attack / Defense multipliers.
struct CombatValueStats_t
{
    CombatRatingStats_t attack;
    CombatRatingStats_t defense;
};

constexpr CombatValueStats_t k_ConventionalValues{
    {StatId_t::ConventionalAttack, StatId_t::Attack},
    {StatId_t::ConventionalDefense, StatId_t::Defense},
};

constexpr CombatValueStats_t k_PsiValues{
    {StatId_t::PsiAttack, StatId_t::Attack},
    {StatId_t::PsiDefense, StatId_t::Defense},
};

// One side of the fight, with the context and morale multiplier its stats resolve under.
struct Fighter_t
{
    const Unit& rUnit;
    EffectContext_t ctx;
    double moraleMultiplier;
};

Fighter_t MakeFighter_(const Unit& rUnit, const Unit& rAttacker, const Unit& rDefender,
                       CombatRole_t role, const MoraleCalculator& rMorale)
{
    EffectContext_t ctx{&rDefender.GetTile(), role};
    ctx.pAttacker = &rAttacker;
    ctx.pDefender = &rDefender;
    ctx.pPartnerFaction = role == CombatRole_t::Attacker ? &rDefender.GetFaction()
                                                  : &rAttacker.GetFaction();
    const int level = CombatMoraleLevel(rUnit, ctx, rMorale);
    return Fighter_t{rUnit, ctx, rMorale.CombatMultiplier(level)};
}

// An attacker with less than one move point left strikes at that fraction of its strength.
int PartialMoveAttack_(const Unit& rAttacker, int strength)
{
    const int perPoint = MovementConstants_t::k_moveFragmentsPerPoint;
    const int remainingFragments = rAttacker.GetMoveFragmentsRemaining();
    if (remainingFragments >= perPoint
        || ResolveFlag(rAttacker, RuleFlagId_t::IgnoresPartialMovePenalty))
    {
        return strength;
    }
    return strength * remainingFragments / perPoint;
}

double AttackHandicap_(const Fighter_t& rAttacker)
{
    const double seed = SeedFor(StatId_t::AttackHandicap);
    return ResolveMultiplicativeStat(rAttacker.rUnit, StatId_t::AttackHandicap, seed,
                                     rAttacker.ctx);
}

int HitsToKill_(int hp, int damagePerHit)
{
    if (hp <= 0)
    {
        return 0;
    }
    return (hp + damagePerHit - 1) / damagePerHit;
}

CombatOdds_t ReduceOdds_(int attackerProduct, int defenderProduct)
{
    if (attackerProduct == 0 && defenderProduct == 0)
    {
        return {};
    }
    const int divisor = std::gcd(attackerProduct, defenderProduct);
    CombatOdds_t odds;
    odds.attackerOdds = attackerProduct / divisor;
    odds.defenderOdds = defenderProduct / divisor;
    return odds;
}

} // namespace

CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement)
{
    CombatStrength_t strength;
    const Fighter_t attacker =
        MakeFighter_(rAttacker, rAttacker, rDefender, CombatRole_t::Attacker, rMorale);
    const Fighter_t defender =
        MakeFighter_(rDefender, rAttacker, rDefender, CombatRole_t::Defender, rMorale);
    strength.bPsiCombat = IsPsiCombat_(rAttacker, rDefender);
    const CombatValueStats_t& values = strength.bPsiCombat ? k_PsiValues : k_ConventionalValues;
    const CombatRatingStats_t& defenderStats =
        engagement == CombatEngagement_t::ArtilleryDuel ? values.attack : values.defense;
    const double attackRating = ResolveCombatRating(rAttacker, values.attack, attacker.ctx,
                                                    attacker.moraleMultiplier);
    const double defenseRating = ResolveCombatRating(rDefender, defenderStats, defender.ctx,
                                                     defender.moraleMultiplier);

    strength.attackStrength = FloorResolvedStat(attackRating * k_CombatStrengthScale);
    strength.defenseStrength = FloorResolvedStat(defenseRating * k_CombatStrengthScale);
    strength.attackStrength = PartialMoveAttack_(rAttacker, strength.attackStrength);
    const double handicapped = strength.attackStrength * AttackHandicap_(attacker);
    strength.attackStrength = FloorResolvedStat(handicapped);
    return strength;
}

CombatRoundOutcome_t ResolveCombatRound(const Unit& rAttacker, const Unit& rDefender,
                                        int attackRoll, int defenseRoll)
{
    const bool bPsiCombat = IsPsiCombat_(rAttacker, rDefender);
    CombatRoundOutcome_t outcome;
    if (attackRoll > defenseRoll)
    {
        outcome.winner = CombatSide_t::Attacker;
        outcome.damage = RoundDamage_(rDefender, bPsiCombat);
        outcome.defenderHpLoss = outcome.damage;
        return outcome;
    }
    outcome.winner = CombatSide_t::Defender;
    outcome.damage = RoundDamage_(rAttacker, bPsiCombat);
    outcome.attackerHpLoss = outcome.damage;
    return outcome;
}

CombatRoundOutcome_t ResolveStrikeRound(const Unit& rAttacker, const Unit& rDefender,
                                        int attackRoll, int defenseRoll, int hpFloor)
{
    CombatRoundOutcome_t outcome = ResolveCombatRound(rAttacker, rDefender, attackRoll, defenseRoll);
    outcome.attackerHpLoss = 0;
    const int current = rDefender.GetCurrentHp();
    const int floorHp = std::min(current, hpFloor);
    outcome.defenderHpLoss = current - std::max(current - outcome.defenderHpLoss, floorHp);
    return outcome;
}

int BombardHpFloor(const Unit& rTarget)
{
    const int hitPoints = ResolveStat(rTarget, StatId_t::HitPoints);
    const std::vector<ActiveEffect_t> effects =
        CurrentGameState().GetTileEffects().CollectAreaEffects(rTarget.GetTile());
    const double percentTotal = ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::BombardMinHpPercent), 0.0, nullptr).total;
    const int percent = FinalizeResolvedStat(percentTotal);
    const double floorHp = static_cast<double>(hitPoints) * static_cast<double>(percent) / 100.0;
    return static_cast<int>(std::ceil(floorHp));
}

CombatOdds_t ComputeCombatOdds(const Unit& rAttacker, const Unit& rDefender,
                               CombatEngagement_t engagement)
{
    const GameState& rGame = CurrentGameState();
    const MoraleCalculator& rMorale = *rGame.GetGameData().moraleCalculator;
    const CombatStrength_t strength =
        ResolveCombatStrength(rAttacker, rDefender, rMorale, engagement);
    const int attackerHits =
        HitsToKill_(rAttacker.GetCurrentHp(), RoundDamage_(rAttacker, strength.bPsiCombat));
    const int defenderHits =
        HitsToKill_(rDefender.GetCurrentHp(), RoundDamage_(rDefender, strength.bPsiCombat));
    const int attackerProduct = strength.attackStrength * attackerHits;
    const int defenderProduct = strength.defenseStrength * defenderHits;
    return ReduceOdds_(attackerProduct, defenderProduct);
}

double DefenderWinChance(const Unit& rAttacker, const Unit& rDefender)
{
    const CombatOdds_t odds =
        ComputeCombatOdds(rAttacker, rDefender, CombatEngagement_t::Standard);
    const int total = odds.attackerOdds + odds.defenderOdds;
    return static_cast<double>(odds.defenderOdds) / static_cast<double>(total);
}

} // namespace ac
