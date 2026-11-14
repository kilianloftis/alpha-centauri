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
    ctx.pPartnerFaction = role == CombatRole_t::Attacker ? &rDefender.GetFaction()
                                                  : &rAttacker.GetFaction();
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
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement)
{
    CombatStrength_t strength;
    const Fighter_t attacker =
        MakeFighter_(rAttacker, rAttacker, rDefender, CombatRole_t::Attacker, rMorale);
    const Fighter_t defender =
        MakeFighter_(rDefender, rAttacker, rDefender, CombatRole_t::Defender, rMorale);
    const StatId_t defenderStat = engagement == CombatEngagement_t::ArtilleryDuel
                                      ? StatId_t::Attack
                                      : StatId_t::Defense;
    strength.bPsiCombat = IsPsiCombat_(rAttacker, rDefender);
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
    const CombatStrength_t strength = ResolveCombatStrength(
        rAttacker, rDefender, rGame.GetTileEffects(), rMorale, engagement);
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
