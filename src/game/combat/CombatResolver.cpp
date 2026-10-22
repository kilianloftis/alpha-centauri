#include "game/combat/CombatResolver.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/combat/CombatStrength.h"
#include "game/movement/MoveCostRules.h"
#include "game/combat/PlanetPearls.h"
#include "game/combat/StackCollateral.h"
#include "game/movement/StepEvaluator.h"
#include "game/effects/TileEffectsContext.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace ac
{

CombatResolver::CombatResolver(const MoveCostRules& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects,
                               const MoraleCalculator& rMorale,
                               std::mt19937& rRng)
    : m_disengage(rMoveCosts, rSteps, rWorldMap)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rMorale(rMorale)
    , m_rRng(rRng)
{
}

int CombatResolver::Roll_(int strength) const
{
    if (strength <= 0)
    {
        return 0;
    }
    std::uniform_int_distribution<int> dist(0, strength - 1);
    return dist(m_rRng);
}

bool CombatResolver::TryDisengage_(Unit& rCandidate, CombatSide_t side, int startHp,
                                   bool bMayDisengage, CombatResult_t& rResult)
{
    if (!bMayDisengage || rCandidate.GetCurrentHp() <= 0)
    {
        return false;
    }
    if (!PastDisengageThreshold(rCandidate, startHp))
    {
        return false;
    }

    const std::vector<const Tile*> retreats = m_disengage.CollectRetreatTiles(rCandidate);
    if (retreats.empty())
    {
        return false;
    }

    // Rolled before the move, with a combat context so conditioned disengage_chance modifiers
    // apply.
    // TODO: rolled per round past the half-HP gate, so the chance compounds; whether SMAC rolls per
    // round or once per combat is unconfirmed.
    const EffectContext_t disengageCtx{
        &rCandidate.GetTile(),
        side == CombatSide_t::Attacker ? CombatRole_t::Attacker : CombatRole_t::Defender};
    if (!RollPercent(ResolveStat(rCandidate, StatId_t::DisengageChance, disengageCtx), m_rRng))
    {
        return false;
    }

    std::uniform_int_distribution<size_t> pick(0, retreats.size() - 1);
    const Tile* pRetreat = retreats[pick(m_rRng)];
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

int CombatResolver::BombardHpFloor_(const Unit& rDefender) const
{
    const int hitPoints = ResolveStat(rDefender, StatId_t::HitPoints);
    const std::vector<ActiveEffect_t> effects =
        m_rTileEffects.CollectAreaEffects(rDefender.GetTile());
    const int percent = FinalizeResolvedStat(ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::BombardMinHpPercent), 0.0, nullptr).total);
    return static_cast<int>(std::ceil(static_cast<double>(hitPoints)
                                      * static_cast<double>(percent) / 100.0));
}

CombatResult_t CombatResolver::Resolve(Unit& rAttacker, Unit& rDefender,
                                       const CombatResolveOptions_t& rOptions)
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();

    const bool bStrike = rOptions.engagement == CombatEngagement_t::ArtilleryStrike;
    const bool bArtillery = bStrike
                            || rOptions.engagement == CombatEngagement_t::ArtilleryDuel;

    const CombatStrength_t strength = ResolveCombatStrength(
        rAttacker, rDefender, m_rTileEffects, m_rMorale, rOptions.engagement);
    result.attackStrength = strength.attackStrength;
    result.defenseStrength = strength.defenseStrength;
    result.bPsiCombat = strength.bPsiCombat;

    const int attackerStartHp = rAttacker.GetCurrentHp();
    const int defenderStartHp = rDefender.GetCurrentHp();
    const bool bAttackerMayDisengage =
        !bArtillery && m_disengage.CanDisengage(rAttacker, rDefender);
    const bool bDefenderMayDisengage =
        !bArtillery && m_disengage.CanDisengage(rDefender, rAttacker);
    const int hpFloor = bStrike ? BombardHpFloor_(rDefender) : 0;

    int roundsPlayed = 0;
    while (rAttacker.GetCurrentHp() > 0 && rDefender.GetCurrentHp() > 0)
    {
        if (bStrike && roundsPlayed >= 1)
        {
            break;
        }

        CombatRound_t round;
        round.attackRoll = Roll_(result.attackStrength);
        round.defenseRoll = Roll_(result.defenseStrength);
        const CombatRoundOutcome_t outcome = ResolveCombatRound(
            rAttacker, rDefender, round.attackRoll, round.defenseRoll, rOptions.engagement,
            hpFloor);
        round.roundWinner = outcome.winner;
        round.damage = outcome.damage;
        // Only the loser is written; a losing strike attacker is left as it was.
        if (outcome.winner == CombatSide_t::Attacker)
        {
            rDefender.SetCurrentHp(rDefender.GetCurrentHp() - outcome.defenderHpLoss);
        }
        else if (outcome.attackerHpLoss > 0)
        {
            rAttacker.SetCurrentHp(rAttacker.GetCurrentHp() - outcome.attackerHpLoss);
        }

        round.attackerHpAfter = rAttacker.GetCurrentHp();
        round.defenderHpAfter = rDefender.GetCurrentHp();
        result.rounds.push_back(round);
        ++roundsPlayed;

        if (bArtillery)
        {
            continue;
        }

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
        ApplyStackCollateral(m_rMorale, rAttacker, rDefender);
        GrantPlanetPearls(m_rMorale, rAttacker.GetFaction(), rDefender);
        rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
    }
    if (result.bAttackerDestroyed)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    }

    return result;
}

} // namespace ac
