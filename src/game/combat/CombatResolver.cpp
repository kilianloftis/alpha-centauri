#include "game/combat/CombatResolver.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/combat/CaptureMutations.h"
#include "game/combat/CaptureRules.h"
#include "game/combat/MoraleRules.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/StepEvaluator.h"
#include "game/effects/TileEffectsContext.h"
#include "lib/RandomRoll.h"

#include <cmath>
#include <optional>
#include <random>
#include <stdexcept>

namespace ac
{

namespace
{

void RecordCasualties_(const Unit& rAttacker, const Unit& rDefender, CombatResult_t& rResult)
{
    rResult.bAttackerDestroyed = rAttacker.GetCurrentHp() <= 0;
    rResult.bDefenderDestroyed = rDefender.GetCurrentHp() <= 0;
    rResult.victor = rResult.bAttackerDestroyed ? CombatSide_t::Defender : CombatSide_t::Attacker;
}

// Writes the round's damage to its loser, and the outcome and both HP to rRound for playback.
void ApplyRound_(Unit& rAttacker, Unit& rDefender, const CombatRoundOutcome_t& rOutcome,
                 CombatRound_t& rRound)
{
    rRound.roundWinner = rOutcome.winner;
    rRound.damage = rOutcome.damage;
    // Only the loser is written; a losing strike attacker is left as it was.
    if (rOutcome.winner == CombatSide_t::Attacker)
    {
        rDefender.SetCurrentHp(rDefender.GetCurrentHp() - rOutcome.defenderHpLoss);
    }
    else if (rOutcome.attackerHpLoss > 0)
    {
        rAttacker.SetCurrentHp(rAttacker.GetCurrentHp() - rOutcome.attackerHpLoss);
    }
    rRound.attackerHpAfter = rAttacker.GetCurrentHp();
    rRound.defenderHpAfter = rDefender.GetCurrentHp();
}

void AppendCapturedIds_(const std::vector<CapturedUnit_t>& rCaptured, std::vector<UnitId_t>& rIds)
{
    for (const CapturedUnit_t& rUnit : rCaptured)
    {
        rIds.push_back(rUnit.pUnit->GetUnitId());
    }
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

CombatResult_t CombatResolver::Resolve(Unit& rAttacker, Unit& rDefender)
{
    const FightExits_t exits{.bAttackerMayDisengage = m_disengage.CanDisengage(rAttacker, rDefender),
                             .bDefenderMayDisengage = m_disengage.CanDisengage(rDefender, rAttacker),
                             .boardingHpToLose = BoardingThreshold(rAttacker, rDefender)};
    return Fight_(rAttacker, rDefender, CombatEngagement_t::Standard, exits);
}

CombatResult_t CombatResolver::ResolveDuel(Unit& rAttacker, Unit& rDefender)
{
    // Artillery never withdraws (terranx-map.md "Disengage"), and boarding is a melee assault.
    return Fight_(rAttacker, rDefender, CombatEngagement_t::ArtilleryDuel, FightExits_t{});
}

CombatResult_t CombatResolver::Strike(Unit& rAttacker, Unit& rTarget)
{
    CombatResult_t result = BeginResult_(rAttacker, rTarget, CombatEngagement_t::ArtilleryStrike);
    const int hpFloor = BombardHpFloor(rTarget);
    result.rounds.push_back(PlayStrikeRound_(rAttacker, rTarget, result, hpFloor));
    RecordCasualties_(rAttacker, rTarget, result);
    // Only a floor of 0 lets a strike kill. Its dead are just removed: no capture, collateral or
    // Planet Pearls (terranx-map.md "Artillery strike vs duel").
    if (result.bDefenderDestroyed)
    {
        rTarget.GetFaction().GetUnitManager().DestroyUnit(rTarget);
    }
    return result;
}

CombatResult_t CombatResolver::BeginResult_(const Unit& rAttacker, const Unit& rDefender,
                                            CombatEngagement_t engagement) const
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();
    result.defenderFactionId = rDefender.GetFaction().GetFactionId();
    result.bKillEarnsPromotion = KillEarnsPromotion(rDefender);
    const CombatStrength_t strength =
        ResolveCombatStrength(rAttacker, rDefender, m_rTileEffects, m_rMorale, engagement);
    result.attackStrength = strength.attackStrength;
    result.defenseStrength = strength.defenseStrength;
    result.bPsiCombat = strength.bPsiCombat;
    return result;
}

CombatResult_t CombatResolver::Fight_(Unit& rAttacker, Unit& rDefender,
                                      CombatEngagement_t engagement, const FightExits_t& rExits)
{
    CombatResult_t result = BeginResult_(rAttacker, rDefender, engagement);
    const Combatant_t attacker{rAttacker, CombatSide_t::Attacker, rAttacker.GetCurrentHp(),
                               rExits.bAttackerMayDisengage};
    const Combatant_t defender{rDefender, CombatSide_t::Defender, rDefender.GetCurrentHp(),
                               rExits.bDefenderMayDisengage};
    const FightEnd_t end = FightRounds_(attacker, defender, rExits.boardingHpToLose, result);
    Finish_(rAttacker, rDefender, end, result);
    return result;
}

CombatResolver::FightEnd_t CombatResolver::FightRounds_(const Combatant_t& rAttacker,
                                                        const Combatant_t& rDefender,
                                                        std::optional<int> boardingHpToLose,
                                                        CombatResult_t& rResult)
{
    while (rAttacker.rUnit.GetCurrentHp() > 0 && rDefender.rUnit.GetCurrentHp() > 0)
    {
        const CombatRound_t round = PlayRound_(rAttacker.rUnit, rDefender.rUnit, rResult);
        rResult.rounds.push_back(round);
        const std::optional<FightEnd_t> early =
            EarlyEnd_(rAttacker, rDefender, round, boardingHpToLose);
        if (early)
        {
            return *early;
        }
    }
    return FightEnd_t{};
}

std::optional<CombatResolver::FightEnd_t> CombatResolver::EarlyEnd_(
    const Combatant_t& rAttacker, const Combatant_t& rDefender, const CombatRound_t& rRound,
    std::optional<int>& rBoardingHpToLose)
{
    // A death ends the fight on its own.
    if (rAttacker.rUnit.GetCurrentHp() <= 0 || rDefender.rUnit.GetCurrentHp() <= 0)
    {
        return std::nullopt;
    }
    const bool bDefenderHit = rRound.roundWinner == CombatSide_t::Attacker;
    if (bDefenderHit
        && BoardingRollWon_(rAttacker.rUnit, rDefender.rUnit, rRound.damage, rBoardingHpToLose))
    {
        return FightEnd_t{.kind = FightEndKind_t::Boarding};
    }
    const Combatant_t& rHit = bDefenderHit ? rDefender : rAttacker;
    const Combatant_t& rOther = bDefenderHit ? rAttacker : rDefender;
    const std::optional<FightEnd_t> hitWithdraws = ChooseWithdrawal_(rHit);
    if (hitWithdraws)
    {
        return hitWithdraws;
    }
    return ChooseWithdrawal_(rOther);
}

std::optional<CombatResolver::FightEnd_t> CombatResolver::ChooseWithdrawal_(
    const Combatant_t& rCandidate)
{
    const Unit& rUnit = rCandidate.rUnit;
    if (!rCandidate.bMayDisengage || !PastDisengageThreshold(rUnit, rCandidate.startHp))
    {
        return std::nullopt;
    }
    const std::vector<const Tile*> retreats = m_disengage.CollectRetreatTiles(rUnit);
    if (retreats.empty())
    {
        return std::nullopt;
    }

    // Rolled with a combat context so conditioned disengage_chance modifiers apply.
    // TODO: SMAC has no disengage roll and only the defender withdraws: one deterministic try
    // once it has lost (HP + 1) / 2, with no attacker withdrawal (terranx-map.md "Disengage").
    // Ours rolls disengage_chance every round past the gate, for either side.
    const CombatRole_t role = rCandidate.side == CombatSide_t::Attacker ? CombatRole_t::Attacker
                                                                        : CombatRole_t::Defender;
    const EffectContext_t disengageCtx{&rUnit.GetTile(), role};
    if (!RollPercent(ResolveStat(rUnit, StatId_t::DisengageChance, disengageCtx), m_rRng))
    {
        return std::nullopt;
    }
    std::uniform_int_distribution<size_t> pick(0, retreats.size() - 1);
    return FightEnd_t{.kind = FightEndKind_t::Withdrawal,
                      .withdrawingSide = rCandidate.side,
                      .pRetreatTile = retreats[pick(m_rRng)]};
}

bool CombatResolver::BoardingRollWon_(const Unit& rAttacker, const Unit& rDefender, int damage,
                                      std::optional<int>& rHpToLose) const
{
    if (!rHpToLose)
    {
        return false;
    }
    // Left below zero after a failed roll, so every later lost round rolls again.
    *rHpToLose -= damage;
    return *rHpToLose <= 0 && RollPercent(BoardingChance(rAttacker, rDefender), m_rRng);
}

void CombatResolver::Finish_(Unit& rAttacker, Unit& rDefender, const FightEnd_t& rEnd,
                             CombatResult_t& rResult)
{
    switch (rEnd.kind)
    {
    case FightEndKind_t::Death:
        RecordCasualties_(rAttacker, rDefender, rResult);
        RemoveLosers_(rAttacker, rDefender, rResult);
        return;
    case FightEndKind_t::Withdrawal:
    {
        Unit& rWithdrawing =
            rEnd.withdrawingSide == CombatSide_t::Attacker ? rAttacker : rDefender;
        Withdraw_(rWithdrawing, rEnd, rResult);
        return;
    }
    case FightEndKind_t::Boarding:
        Board_(rAttacker, rDefender, rResult);
        return;
    }
    throw std::logic_error("CombatResolver: unknown FightEndKind_t");
}

void CombatResolver::Withdraw_(Unit& rWithdrawing, const FightEnd_t& rEnd,
                               CombatResult_t& rResult)
{
    m_rWorldMap.GetUnitPositions().MoveUnit(rWithdrawing, *rEnd.pRetreatTile);
    rResult.pRetreatTile = rEnd.pRetreatTile;
    const bool bAttackerWithdrew = rEnd.withdrawingSide == CombatSide_t::Attacker;
    rResult.bAttackerDisengaged = bAttackerWithdrew;
    rResult.bDefenderDisengaged = !bAttackerWithdrew;
    rResult.victor = bAttackerWithdrew ? CombatSide_t::Defender : CombatSide_t::Attacker;
}

void CombatResolver::Board_(Unit& rAttacker, const Unit& rDefender, CombatResult_t& rResult)
{
    const std::vector<CapturedUnit_t> captured = BoardingCapture(rAttacker, rDefender.GetTile());
    CaptureUnits(m_rWorldMap.GetUnitPositions(), rAttacker.GetFaction(), captured);
    AppendCapturedIds_(captured, rResult.capturedUnitIds);
    rResult.bDefenderBoarded = true;
    rResult.victor = CombatSide_t::Attacker;
}

void CombatResolver::RemoveLosers_(Unit& rAttacker, Unit& rDefender, CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed)
    {
        const std::optional<UnitId_t> capturedId =
            RemoveDefeatedUnit(m_rWorldMap.GetUnitPositions(), rAttacker, rDefender);
        if (capturedId)
        {
            rResult.capturedUnitIds.push_back(*capturedId);
        }
    }
    if (rResult.bAttackerDestroyed)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    }
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

CombatRound_t CombatResolver::RollRound_(const CombatResult_t& rResult) const
{
    CombatRound_t round;
    round.attackRoll = Roll_(rResult.attackStrength);
    round.defenseRoll = Roll_(rResult.defenseStrength);
    return round;
}

CombatRound_t CombatResolver::PlayRound_(Unit& rAttacker, Unit& rDefender,
                                         const CombatResult_t& rResult)
{
    CombatRound_t round = RollRound_(rResult);
    const CombatRoundOutcome_t outcome =
        ResolveCombatRound(rAttacker, rDefender, round.attackRoll, round.defenseRoll);
    ApplyRound_(rAttacker, rDefender, outcome, round);
    return round;
}

CombatRound_t CombatResolver::PlayStrikeRound_(Unit& rAttacker, Unit& rTarget,
                                               const CombatResult_t& rResult, int hpFloor)
{
    CombatRound_t round = RollRound_(rResult);
    const CombatRoundOutcome_t outcome =
        ResolveStrikeRound(rAttacker, rTarget, round.attackRoll, round.defenseRoll, hpFloor);
    ApplyRound_(rAttacker, rTarget, outcome, round);
    return round;
}

} // namespace ac
