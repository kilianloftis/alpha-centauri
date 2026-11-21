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

// Standard: attacker psi weapon or defender psi armor. Strike: attacker weapon only. Duel:
// either side's weapon (weapon-vs-weapon). Shipping Spore outcomes match terranx-map.md
// "Psi engagement"; see config/deviations/README.md.
const CombatKind_t& CombatKindFor_(const Unit& rAttacker, const Unit& rDefender,
                                   CombatEngagement_t engagement)
{
    const bool bAttackerPsi = ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat);
    switch (engagement)
    {
    case CombatEngagement_t::ArtilleryStrike:
        if (bAttackerPsi)
        {
            return k_PsiCombat;
        }
        return k_ConventionalCombat;
    case CombatEngagement_t::ArtilleryDuel:
        if (bAttackerPsi || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat))
        {
            return k_PsiCombat;
        }
        return k_ConventionalCombat;
    case CombatEngagement_t::Standard:
        if (bAttackerPsi || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiDefense))
        {
            return k_PsiCombat;
        }
        return k_ConventionalCombat;
    default:
        throw std::invalid_argument("Invalid engagement");
    }
}

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

// HP rLoser loses per round rWinner wins. At least 1, so every fight ends.
int RoundDamage_(const Fighter_t& rWinner, const Fighter_t& rLoser, const CombatKind_t& rKind)
{
    const double dealt = ResolveUnitStat(CollectLiveUnitEffects(rWinner.rUnit), rKind.damage,
                                         SeedFor(rKind.damage), &rWinner.ctx);
    const double taken = ResolveMultiplicativeStat(rLoser.rUnit, rKind.damageTaken,
                                                   SeedFor(rKind.damageTaken), rLoser.ctx);
    return std::max(1, FinalizeResolvedStat(dealt * taken));
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

int ApplyAttackHandicap_(const Fighter_t& rAttacker, int strength)
{
    const UnitEffects_t effects = CollectLiveUnitEffects(rAttacker.rUnit);
    const double seed = static_cast<double>(strength);
    const double handicapped =
        ResolveUnitStat(effects, StatId_t::AttackHandicap, seed, &rAttacker.ctx);
    return FloorResolvedStat(handicapped);
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

CombatRatingStats_t DefenseRatingStats(const CombatKind_t& rKind)
{
    const CombatConfig_t& rConfig = *CurrentGameState().GetGameData().combatConfig;
    CombatRatingStats_t stats = rKind.defense;
    const auto it = rConfig.defenseSlotsByKind.find(rKind.id);
    if (it != rConfig.defenseSlotsByKind.end())
    {
        stats.slots = it->second;
    }
    return stats;
}

int ResolveFighterStrength(const Unit& rUnit, CombatRatingStats_t stats,
                           const EffectContext_t& rCtx, double moraleMultiplier)
{
    const double rating = ResolveCombatRating(rUnit, stats, rCtx, moraleMultiplier);
    const int scaled = FloorResolvedStat(rating * k_CombatStrengthScale);
    return ApplyCombatStrengthSteps(scaled, rUnit, stats, rCtx);
}

CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement)
{
    CombatStrength_t strength;
    const Fighter_t attacker =
        MakeFighter_(rAttacker, rAttacker, rDefender, CombatRole_t::Attacker, rMorale);
    const Fighter_t defender =
        MakeFighter_(rDefender, rAttacker, rDefender, CombatRole_t::Defender, rMorale);
    const CombatKind_t& kind = CombatKindFor_(rAttacker, rDefender, engagement);
    const CombatRatingStats_t defenderStats =
        engagement == CombatEngagement_t::ArtilleryDuel ? kind.attack : DefenseRatingStats(kind);
    strength.attackStrength = ResolveFighterStrength(rAttacker, kind.attack, attacker.ctx,
                                                     attacker.moraleMultiplier);
    strength.defenseStrength = ResolveFighterStrength(rDefender, defenderStats, defender.ctx,
                                                      defender.moraleMultiplier);
    strength.attackStrength = PartialMoveAttack_(rAttacker, strength.attackStrength);
    strength.attackStrength = ApplyAttackHandicap_(attacker, strength.attackStrength);
    strength.attackerRoundDamage = RoundDamage_(defender, attacker, kind);
    strength.defenderRoundDamage = RoundDamage_(attacker, defender, kind);
    return strength;
}

CombatRoundOutcome_t ResolveCombatRound(const CombatStrength_t& rStrength, int attackRoll,
                                        int defenseRoll)
{
    CombatRoundOutcome_t outcome;
    if (attackRoll > defenseRoll)
    {
        outcome.winner = CombatSide_t::Attacker;
        outcome.damage = rStrength.defenderRoundDamage;
        outcome.defenderHpLoss = outcome.damage;
        return outcome;
    }
    outcome.winner = CombatSide_t::Defender;
    outcome.damage = rStrength.attackerRoundDamage;
    outcome.attackerHpLoss = outcome.damage;
    return outcome;
}

CombatRoundOutcome_t ResolveStrikeRound(const CombatStrength_t& rStrength, const Unit& rDefender,
                                        int attackRoll, int defenseRoll, int hpFloor)
{
    CombatRoundOutcome_t outcome = ResolveCombatRound(rStrength, attackRoll, defenseRoll);
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
    const int attackerHits = HitsToKill_(rAttacker.GetCurrentHp(), strength.attackerRoundDamage);
    const int defenderHits = HitsToKill_(rDefender.GetCurrentHp(), strength.defenderRoundDamage);
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
