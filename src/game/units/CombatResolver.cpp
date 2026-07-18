#include "game/units/CombatResolver.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitManager.h"
#include "game/Faction.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/StepEvaluator.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace ac
{

CombatResolver::CombatResolver(const MoveCostCalculator& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects)
    : CombatResolver(rMoveCosts, rSteps, rWorldMap, rTileEffects, std::random_device{}())
{
}

CombatResolver::CombatResolver(const MoveCostCalculator& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects,
                               uint32_t seed)
    : m_disengage(rMoveCosts, rSteps, rWorldMap)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rng(seed)
{
}

int CombatResolver::Roll_(int strength) const
{
    if (strength <= 0)
    {
        return 0;
    }
    std::uniform_int_distribution<int> dist(0, strength - 1);
    return dist(m_rng);
}

bool CombatResolver::TryDisengage_(Unit& rCandidate, CombatSide_t side, int startHp,
                                   bool bMayDisengage, CombatResult_t& rResult)
{
    if (!bMayDisengage || rCandidate.GetCurrentHp() <= 0)
    {
        return false;
    }
    if ((startHp - rCandidate.GetCurrentHp()) * 2 < startHp)
    {
        return false;
    }

    const std::vector<const Tile*> retreats = m_disengage.CollectRetreatTiles(rCandidate);
    if (retreats.empty())
    {
        return false;
    }

    std::uniform_int_distribution<size_t> pick(0, retreats.size() - 1);
    const Tile* pRetreat = retreats[pick(m_rng)];
    m_rWorldMap.GetUnitPositions().MoveUnit(rCandidate, *pRetreat);
    rResult.pRetreatTile = pRetreat;
    if (side == CombatSide_t::Attacker)
    {
        rResult.bAttackerDisengaged = true;
    }
    else
    {
        rResult.bDefenderDisengaged = true;
    }
    return true;
}

CombatResult_t CombatResolver::Resolve(Unit& rAttacker, Unit& rDefender)
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();

    const EffectContext_t attackCtx{&rDefender.GetTile()};
    const double tileDefenseMult = m_rTileEffects.ResolveTileDefenseMultiplier(
        rDefender.GetTile(), rDefender.GetFaction().GetFactionId());

    result.bPsiCombat = ResolveFlag(rAttacker, RuleFlagId_t::ForcesPsiCombat)
                        || ResolveFlag(rDefender, RuleFlagId_t::ForcesPsiCombat);
    if (result.bPsiCombat)
    {
        const double attackRating =
            ResolveMultiplicativeStat(rAttacker, StatId_t::Attack, 1.0, attackCtx);
        const double defenseRating =
            ResolveMultiplicativeStat(rDefender, StatId_t::Defense, 1.0);
        result.attackStrength =
            static_cast<int>(std::lround(attackRating * k_combatStrengthScale));
        result.defenseStrength = static_cast<int>(
            std::lround(defenseRating * tileDefenseMult * k_combatStrengthScale));
    }
    else
    {
        const int attackRating = ResolveStat(rAttacker, StatId_t::Attack, attackCtx);
        const int defenseRating = ResolveStat(rDefender, StatId_t::Defense);
        result.attackStrength = attackRating * k_combatStrengthScale;
        result.defenseStrength = static_cast<int>(
            std::lround(defenseRating * tileDefenseMult * k_combatStrengthScale));
    }

    const int attackerStartHp = rAttacker.GetCurrentHp();
    const int defenderStartHp = rDefender.GetCurrentHp();
    const bool bAttackerMayDisengage = m_disengage.CanDisengage(rAttacker, rDefender);
    const bool bDefenderMayDisengage = m_disengage.CanDisengage(rDefender, rAttacker);

    while (rAttacker.GetCurrentHp() > 0 && rDefender.GetCurrentHp() > 0)
    {
        CombatRound_t round;
        round.attackRoll = Roll_(result.attackStrength);
        round.defenseRoll = Roll_(result.defenseStrength);
        round.damage = k_roundDamage;

        // Strict greater-than for the attacker; equal rolls favour the defender.
        if (round.attackRoll > round.defenseRoll)
        {
            round.roundWinner = CombatSide_t::Attacker;
            round.damage = result.bPsiCombat
                               ? std::max(1, ResolveStat(rDefender, StatId_t::PsiDamage))
                               : k_roundDamage;
            rDefender.SetCurrentHp(rDefender.GetCurrentHp() - round.damage);
        }
        else
        {
            round.roundWinner = CombatSide_t::Defender;
            round.damage = result.bPsiCombat
                               ? std::max(1, ResolveStat(rAttacker, StatId_t::PsiDamage))
                               : k_roundDamage;
            rAttacker.SetCurrentHp(rAttacker.GetCurrentHp() - round.damage);
        }

        round.attackerHpAfter = rAttacker.GetCurrentHp();
        round.defenderHpAfter = rDefender.GetCurrentHp();
        result.rounds.push_back(round);

        // The side that just took damage is checked first (it may have newly crossed the
        // half-HP threshold); then the other, in case both already qualify.
        const CombatSide_t damaged =
            round.roundWinner == CombatSide_t::Attacker ? CombatSide_t::Defender
                                                        : CombatSide_t::Attacker;
        const bool bDisengaged =
            damaged == CombatSide_t::Defender
                ? (TryDisengage_(rDefender, CombatSide_t::Defender, defenderStartHp,
                                 bDefenderMayDisengage, result)
                   || TryDisengage_(rAttacker, CombatSide_t::Attacker, attackerStartHp,
                                    bAttackerMayDisengage, result))
                : (TryDisengage_(rAttacker, CombatSide_t::Attacker, attackerStartHp,
                                 bAttackerMayDisengage, result)
                   || TryDisengage_(rDefender, CombatSide_t::Defender, defenderStartHp,
                                    bDefenderMayDisengage, result));
        if (bDisengaged)
        {
            break;
        }
    }

    result.bAttackerDestroyed =
        rAttacker.GetCurrentHp() <= 0 && !result.bAttackerDisengaged;
    result.bDefenderDestroyed =
        rDefender.GetCurrentHp() <= 0 && !result.bDefenderDisengaged;
    if (result.bAttackerDestroyed || result.bAttackerDisengaged)
    {
        result.victor = CombatSide_t::Defender;
    }
    else
    {
        result.victor = CombatSide_t::Attacker;
    }

    if (result.bDefenderDestroyed)
    {
        rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
    }
    if (result.bAttackerDestroyed)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    }

    return result;
}

} // namespace ac
