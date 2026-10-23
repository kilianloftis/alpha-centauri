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

namespace
{

// A unit at 0 HP is destroyed unless it got away first.
void RecordOutcome_(const Unit& rAttacker, const Unit& rDefender, CombatResult_t& rResult)
{
    rResult.bAttackerDestroyed = rAttacker.GetCurrentHp() <= 0 && !rResult.bAttackerDisengaged;
    rResult.bDefenderDestroyed = rDefender.GetCurrentHp() <= 0 && !rResult.bDefenderDisengaged;
    const bool bAttackerLost = rResult.bAttackerDestroyed || rResult.bAttackerDisengaged;
    rResult.victor = bAttackerLost ? CombatSide_t::Defender : CombatSide_t::Attacker;
}

} // namespace

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

bool CombatResolver::TryDisengage_(const Combatant_t& rCombatant, CombatResult_t& rResult)
{
    Unit& rCandidate = rCombatant.rUnit;
    const CombatSide_t side = rCombatant.side;
    if (!rCombatant.bMayDisengage || rCandidate.GetCurrentHp() <= 0)
    {
        return false;
    }
    if (!PastDisengageThreshold(rCandidate, rCombatant.startHp))
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
    const double percentTotal = ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::BombardMinHpPercent), 0.0, nullptr).total;
    const int percent = FinalizeResolvedStat(percentTotal);
    const double floorHp = static_cast<double>(hitPoints) * static_cast<double>(percent) / 100.0;
    return static_cast<int>(std::ceil(floorHp));
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

    const Combatant_t attacker{rAttacker, CombatSide_t::Attacker, rAttacker.GetCurrentHp(),
                               !bArtillery && m_disengage.CanDisengage(rAttacker, rDefender)};
    const Combatant_t defender{rDefender, CombatSide_t::Defender, rDefender.GetCurrentHp(),
                               !bArtillery && m_disengage.CanDisengage(rDefender, rAttacker)};
    const int hpFloor = bStrike ? BombardHpFloor_(rDefender) : 0;

    while (rAttacker.GetCurrentHp() > 0 && rDefender.GetCurrentHp() > 0)
    {
        if (bStrike && !result.rounds.empty())
        {
            break;
        }
        const CombatRound_t round =
            PlayRound_(rAttacker, rDefender, result, rOptions.engagement, hpFloor);
        result.rounds.push_back(round);
        if (bArtillery)
        {
            continue;
        }
        // The side that just took damage is checked first (it may have newly crossed the
        // half-HP threshold); then the other, in case both already qualify.
        const bool bDefenderHit = round.roundWinner == CombatSide_t::Attacker;
        const Combatant_t& rHit = bDefenderHit ? defender : attacker;
        const Combatant_t& rOther = bDefenderHit ? attacker : defender;
        if (TryDisengage_(rHit, result) || TryDisengage_(rOther, result))
        {
            break;
        }
    }

    RecordOutcome_(rAttacker, rDefender, result);
    DestroyLosers_(rAttacker, rDefender, result);
    return result;
}

CombatRound_t CombatResolver::PlayRound_(Unit& rAttacker, Unit& rDefender,
                                         const CombatResult_t& rResult,
                                         CombatEngagement_t engagement, int hpFloor)
{
    CombatRound_t round;
    round.attackRoll = Roll_(rResult.attackStrength);
    round.defenseRoll = Roll_(rResult.defenseStrength);
    const CombatRoundOutcome_t outcome = ResolveCombatRound(
        rAttacker, rDefender, round.attackRoll, round.defenseRoll, engagement, hpFloor);
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
    return round;
}

void CombatResolver::DestroyLosers_(Unit& rAttacker, Unit& rDefender,
                                    const CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed)
    {
        ApplyStackCollateral(m_rMorale, rAttacker, rDefender);
        GrantPlanetPearls(m_rMorale, rAttacker.GetFaction(), rDefender);
        rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
    }
    if (rResult.bAttackerDestroyed)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    }
}

} // namespace ac
