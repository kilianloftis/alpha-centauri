#include "game/units/UnitAttackExecutor.h"

#include "game/combat/CollateralMutations.h"
#include "game/combat/InterceptMutations.h"
#include "game/combat/MoraleMutations.h"
#include "game/combat/MoraleRules.h"
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
    , m_rMorale(rMorale)
    , m_rRng(rRng)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, rMorale, rRng)
    , m_rWorld(rWorld)
    , m_rOrders(rOrders)
{
}

void UnitAttackExecutor::HaltBombardedTarget_(Unit& rTarget, const CombatResult_t& rCombat)
{
    if (!rCombat.bDefenderDestroyed)
    {
        // TODO: a bombarded unit must not heal this turn, once per-turn healing exists.
        rTarget.ClearOrder();
    }
}

std::optional<CombatResult_t> UnitAttackExecutor::TryAttack(Unit& rAttacker, Unit& rDefender)
{
    if (FindAttackableHostileOnTile(rAttacker, rDefender.GetTile()) != &rDefender)
    {
        return std::nullopt;
    }

    // The aftermath still reads an attacker the fight destroyed.
    const auto destructionScope = rAttacker.GetFaction().GetUnitManager().DeferDestruction();
    // Reported once the attack resolves, so a sneak attack's shared-tile evacuation cannot
    // move either side before the blow lands.
    const std::optional<HostileAct_t> act = AttackHostileAct(rAttacker, rDefender);
    // Read before the fight may destroy the defender.
    const Tile& rBattleTile = rDefender.GetTile();

    CombatResult_t result;
    if (std::optional<CombatResult_t> intercepted =
            m_rWorld.TryInterceptAttack(rAttacker, rDefender, m_rRng))
    {
        // A defender lost to its own intercept was never fought, so no fight aftermath follows.
        result = std::move(*intercepted);
    }
    else
    {
        std::vector<const Tile*> scrambleHops;
        Unit& rCombatDefender = ResolveScrambleDefender_(rAttacker, rDefender, scrambleHops);
        result = m_combat.Resolve(rAttacker, rCombatDefender);
        result.scramblePath = std::move(scrambleHops);
        ApplyFightAftermath_(rAttacker, rCombatDefender, rBattleTile, result);
    }

    if (!result.bAttackerDestroyed)
    {
        result.bAttackerDestroyed = SpendAttack(rAttacker, AttackKind_t::Attack);
    }
    if (act)
    {
        m_rWorld.OnHostileAct(*act);
    }
    return result;
}

void UnitAttackExecutor::ApplyFightAftermath_(Unit& rAttacker, Unit& rDefender,
                                              const Tile& rBattleTile, CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed)
    {
        const std::vector<UnitId_t> captured =
            ApplyStackCollateral(m_rWorldMap.GetUnitPositions(), rAttacker, rBattleTile);
        rResult.capturedUnitIds.insert(rResult.capturedUnitIds.end(), captured.begin(),
                                       captured.end());
    }
    if (const std::optional<CombatSide_t> survivor = PromotionCandidate(rResult))
    {
        Unit& rSurvivor = *survivor == CombatSide_t::Attacker ? rAttacker : rDefender;
        TryPromote(rSurvivor, rResult.attackStrength, rResult.defenseStrength, m_rMorale, m_rRng);
    }
    m_rWorld.ApplyFightConsequences(rAttacker, rBattleTile, rResult);
}

std::optional<UnitAttackExecutor::BombardResult_t> UnitAttackExecutor::TryBombard(
    Unit& rAttacker, const Tile& rTargetTile)
{
    if (!IsWithinBombardRange(rAttacker, rTargetTile))
    {
        return std::nullopt;
    }

    // The duel's aftermath still reads an attacker the duel destroyed.
    const auto destructionScope = rAttacker.GetFaction().GetUnitManager().DeferDestruction();
    const std::vector<std::string> improvementTargets = BombardImprovementTargets(rTargetTile);
    const std::vector<HostileAct_t> acts = BombardHostileActs(rAttacker, rTargetTile);

    BombardResult_t result;
    // A hostile that can bombard back answers with a duel, and then nothing is struck.
    Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTargetTile);
    if (pDuelTarget)
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

    if (!result.bAttackerDestroyed)
    {
        result.bAttackerDestroyed = SpendAttack(rAttacker, AttackKind_t::Bombard);
    }
    for (const HostileAct_t& rAct : acts)
    {
        m_rWorld.OnHostileAct(rAct);
    }
    return result;
}

CombatResult_t UnitAttackExecutor::FireDuel_(Unit& rAttacker, Unit& rDuelTarget)
{
    const Tile& rBattleTile = rDuelTarget.GetTile();
    CombatResult_t duel = m_combat.ResolveDuel(rAttacker, rDuelTarget);
    ApplyFightAftermath_(rAttacker, rDuelTarget, rBattleTile, duel);
    HaltBombardedTarget_(rDuelTarget, duel);
    return duel;
}

std::vector<CombatResult_t> UnitAttackExecutor::FireStrikes_(Unit& rAttacker,
                                                             const std::vector<Unit*>& rTargets)
{
    std::vector<CombatResult_t> strikes;
    for (Unit* pTarget : rTargets)
    {
        CombatResult_t strike = m_combat.Strike(rAttacker, *pTarget);
        HaltBombardedTarget_(*pTarget, strike);
        strikes.push_back(std::move(strike));
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
