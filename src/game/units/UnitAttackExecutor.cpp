#include "game/units/UnitAttackExecutor.h"

#include "game/combat/InterceptMutations.h"
#include "game/combat/ScrambleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitMutations.h"
#include "game/units/UnitOrderExecutor.h"

#include <utility>
#include <random>

namespace ac
{

UnitAttackExecutor::UnitAttackExecutor(const MoveCostRules& rMoveCosts,
                                       const StepEvaluator& rSteps, WorldMap& rWorldMap,
                                       TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale, std::mt19937& rRng,
                                       IUnitOrderWorld& rWorld, UnitOrderExecutor& rOrders)
    : m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rRng(rRng)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, rMorale, rRng)
    , m_rWorld(rWorld)
    , m_rOrders(rOrders)
{
}

bool UnitAttackExecutor::SpendAttackAction_(Unit& rAttacker, AttackKind_t kind)
{
    rAttacker.Latches().MarkAttacked();
    const int fragments = AttackMoveFragments(rAttacker, kind);
    rAttacker.SpendMoveFragments(fragments);
    rAttacker.ClearOrder();
    return ExpendIfSingleUse(rAttacker);
}

bool UnitAttackExecutor::ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile)
{
    return m_rWorld.ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, m_rRng)
        .bActorDestroyed;
}

CombatResult_t UnitAttackExecutor::SettleBombardExchange_(Unit& rDefender, CombatResult_t combat)
{
    combat.bBombardPlayback = true;
    if (!combat.bDefenderDestroyed)
    {
        // TODO: a bombarded unit must not heal this turn, once per-turn healing exists.
        rDefender.ClearOrder();
    }
    return combat;
}

std::optional<CombatResult_t> UnitAttackExecutor::TryAttack(Unit& rAttacker, Unit& rDefender)
{
    if (FindAttackableHostileOnTile(rAttacker, rDefender.GetTile()) != &rDefender)
    {
        return std::nullopt;
    }

    // Reported once the attack resolves, so a sneak attack's shared-tile evacuation cannot
    // move either side before the blow lands.
    const HostileAct_t act = HostileActAgainst(rAttacker, rDefender);
    const bool bHostile = IsHostileAttack(rAttacker, rDefender);
    // Read before the fight may destroy the defender.
    const Tile& rDefenderTile = rDefender.GetTile();

    const std::optional<CombatResult_t> intercepted =
        m_rWorld.TryInterceptAttack(rAttacker, rDefender, m_rRng);
    CombatResult_t result = intercepted ? *intercepted : FightDefender_(rAttacker, rDefender);

    // Combat never moves either side, so capturing an emptied base is a later step onto it.
    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Attack))
    {
        result.bAttackerDestroyed = true;
    }
    // A defender lost to its own intercept was never fought: no last defender lost in battle.
    // A native raider spends itself on the raid, so playback must not show it surviving.
    if (!intercepted && result.bDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rDefenderTile))
    {
        result.bAttackerDestroyed = true;
    }
    if (bHostile)
    {
        ReportHostileAct_(act);
    }
    return result;
}

CombatResult_t UnitAttackExecutor::FightDefender_(Unit& rAttacker, Unit& rDefender)
{
    std::vector<const Tile*> scrambleHops;
    Unit& rCombatDefender = ResolveScrambleDefender_(rAttacker, rDefender, scrambleHops);

    CombatResult_t result = m_combat.Resolve(rAttacker, rCombatDefender);
    result.scramblePath = std::move(scrambleHops);
    ReportCapture_(rAttacker, result);
    return result;
}

void UnitAttackExecutor::ReportHostileAct_(const HostileAct_t& rAct)
{
    m_rWorld.OnHostileAct(rAct);
}

void UnitAttackExecutor::ReportCapture_(const Unit& rAttacker, const CombatResult_t& rResult)
{
    // Only a surviving attacker takes units.
    if (!rResult.capturedUnitIds.empty())
    {
        m_rWorld.OnCombatCapture(rAttacker, rResult);
    }
}

std::optional<UnitAttackExecutor::BombardResult_t> UnitAttackExecutor::TryBombard(
    Unit& rAttacker, const Tile& rTargetTile)
{
    if (!IsWithinBombardRange(rAttacker, rTargetTile))
    {
        return std::nullopt;
    }

    const std::vector<std::string> improvementTargets = BombardImprovementTargets(rTargetTile);
    const std::vector<HostileAct_t> acts = BombardHostileActs(rAttacker, rTargetTile);

    BombardResult_t result;
    // A hostile that can bombard back answers with a duel, and then nothing is struck.
    Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTargetTile);
    const bool bDuel = pDuelTarget != nullptr;
    if (bDuel)
    {
        result.combats.push_back(FireDuel_(rAttacker, *pDuelTarget));
    }
    else
    {
        result.combats = FireStrikes_(rAttacker, BombardStrikeTargets(rAttacker, rTargetTile));
    }
    result.bAttackerDestroyed = !result.combats.empty() && result.combats.back().bAttackerDestroyed;
    if (!improvementTargets.empty())
    {
        result.destroyedImprovementId = DestroyRandomImprovement_(rTargetTile, improvementTargets);
    }

    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Bombard))
    {
        result.bAttackerDestroyed = true;
    }

    // A strike's kill never costs a base its last defender (terranx-map.md "Artillery strike vs
    // duel").
    if (bDuel && result.combats.front().bDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rTargetTile))
    {
        result.bAttackerDestroyed = true;
        result.combats.back().bAttackerDestroyed = true;
    }
    for (const HostileAct_t& rAct : acts)
    {
        ReportHostileAct_(rAct);
    }
    return result;
}

CombatResult_t UnitAttackExecutor::FireDuel_(Unit& rAttacker, Unit& rDuelTarget)
{
    CombatResult_t duel = m_combat.ResolveDuel(rAttacker, rDuelTarget);
    ReportCapture_(rAttacker, duel);
    return SettleBombardExchange_(rDuelTarget, std::move(duel));
}

std::vector<CombatResult_t> UnitAttackExecutor::FireStrikes_(Unit& rAttacker,
                                                             const std::vector<Unit*>& rTargets)
{
    std::vector<CombatResult_t> strikes;
    for (Unit* pTarget : rTargets)
    {
        CombatResult_t strike = m_combat.Strike(rAttacker, *pTarget);
        strikes.push_back(SettleBombardExchange_(*pTarget, std::move(strike)));
    }
    return strikes;
}

std::string UnitAttackExecutor::DestroyRandomImprovement_(
    const Tile& rTile, const std::vector<std::string>& rCandidates)
{
    std::uniform_int_distribution<size_t> pick(0, rCandidates.size() - 1);
    const std::string id = rCandidates[pick(m_rRng)];
    m_rTileEffects.RemoveOccupantWithEffects(m_rWorldMap.MutableTile(rTile), id);
    return id;
}

Unit& UnitAttackExecutor::ResolveScrambleDefender_(Unit& rAttacker,
                                                  Unit& rOriginalDefender,
                                                  std::vector<const Tile*>& rOutPath)
{
    rOutPath.clear();
    Unit* pScrambler = FindScrambler(rAttacker, rOriginalDefender);
    if (!pScrambler)
    {
        return rOriginalDefender;
    }
    if (!ScrambleTo_(*pScrambler, rOriginalDefender.GetTile(), rOutPath))
    {
        rOutPath.clear();
        return rOriginalDefender;
    }
    return *pScrambler;
}

bool UnitAttackExecutor::ScrambleTo_(Unit& rScrambler, const Tile& rDest,
                                    std::vector<const Tile*>& rOutPath)
{
    rScrambler.SetOrder(MoveOrder_t{.pDestination = &rDest});
    auto movedConn = m_rWorldMap.GetUnitPositions().OnUnitMoved.ConnectScoped(
        [&](Unit& rUnit)
        {
            if (&rUnit == &rScrambler)
            {
                rOutPath.push_back(&rUnit.GetTile());
            }
        });

    if (m_rOrders.Execute(rScrambler) == OrderProgress_t::UnitDestroyed)
    {
        return false;
    }
    if (&rScrambler.GetTile() != &rDest)
    {
        rScrambler.ClearOrder();
        return false;
    }
    return true;
}

} // namespace ac
