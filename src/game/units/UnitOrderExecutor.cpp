#include "game/units/UnitOrderExecutor.h"

#include "game/combat/InterceptMutations.h"
#include "game/combat/MoraleMutations.h"
#include "game/combat/MoraleRules.h"
#include "game/combat/ScrambleRules.h"
#include "game/units/UnitOrder.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/MovementRules.h"
#include "game/movement/Pathfinder.h"
#include "game/combat/AttackRules.h"
#include "game/base/BaseConquestRules.h"
#include "game/base/FoundBaseRules.h"
#include "game/base/BaseMutations.h"
#include "game/movement/AirdropRules.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/movement/TransportMutations.h"
#include "game/terraform/TerraformRules.h"
#include "game/terraform/TerraformMutations.h"
#include "game/visibility/FactionRevealedUnits.h"
#include "game/units/UnitManager.h"
#include "game/visibility/VisibilityRules.h"
#include "game/base/BaseManager.h"
#include "game/economy/EconomyManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/map/TerrainOperationRegistry.h"
#include <algorithm>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

// The map's own tile, for callers that hold it const.
Tile& MutableTile_(WorldMap& rWorldMap, const Tile& rTile)
{
    Tile* pTile = rWorldMap.GetTile(rTile.GetX(), rTile.GetY());
    if (!pTile)
    {
        throw std::logic_error("UnitOrderExecutor: tile is not on this map");
    }
    return *pTile;
}

// A move that stops short goes on next turn, unless contact cleared its order.
OrderProgress_t ProgressOfHaltedMove_(const Unit& rUnit)
{
    return rUnit.GetOrder().has_value() ? OrderProgress_t::Continue : OrderProgress_t::Complete;
}

} // namespace

UnitOrderExecutor::UnitOrderExecutor(const MoveCostRules& rMoveCosts,
                                     const StepEvaluator& rSteps,
                                     WorldMap& rWorldMap,
                                     TileEffectsContext& rTileEffects,
                                     Pathfinder& rPathfinder,
                                     const MoraleCalculator& rMorale,
                                     const TerrainOperationRegistry& rTerrainOperations,
                                     std::mt19937& rRng,
                                     IUnitOrderWorld& rWorld)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_rMorale(rMorale)
    , m_rTerrainOperations(rTerrainOperations)
    , m_rRng(rRng)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, rMorale, rRng)
    , m_rWorld(rWorld)
{
}

OrderProgress_t UnitOrderExecutor::ExpendIfSingleUse_(const Unit& rUnit) const
{
    return ResolveFlag(rUnit, RuleFlagId_t::SingleUse) ? OrderProgress_t::Expended
                                                       : OrderProgress_t::Complete;
}

void UnitOrderExecutor::PromoteOnKill_(Unit& rAttacker, Unit& rDefender,
                                       const CombatResult_t& rResult)
{
    const std::optional<CombatSide_t> candidate = PromotionCandidate(rResult);
    if (!candidate)
    {
        return;
    }
    Unit& rSurvivor = *candidate == CombatSide_t::Attacker ? rAttacker : rDefender;
    TryPromote(rSurvivor, rResult.attackStrength, rResult.defenseStrength, m_rMorale, m_rRng);
}

bool UnitOrderExecutor::SpendAttackAction_(Unit& rAttacker, AttackKind_t kind)
{
    rAttacker.MarkAttacked();
    const int fragments = AttackMoveFragments(rAttacker, kind);
    rAttacker.SpendMoveFragments(fragments);
    rAttacker.ClearOrder();
    if (ExpendIfSingleUse_(rAttacker) == OrderProgress_t::Expended)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
        return true;
    }
    return false;
}

bool UnitOrderExecutor::ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile)
{
    return m_rWorld.ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, m_rRng)
        .bActorDestroyed;
}

CombatResult_t UnitOrderExecutor::ResolveBombardExchange_(Unit& rAttacker, Unit& rDefender,
                                                         CombatEngagement_t engagement)
{
    CombatResolveOptions_t options;
    options.engagement = engagement;
    CombatResult_t combat = m_combat.Resolve(rAttacker, rDefender, options);
    combat.bBombardPlayback = true;
    PromoteOnKill_(rAttacker, rDefender, combat);
    if (!combat.bDefenderDestroyed)
    {
        // TODO: a bombarded unit must not heal this turn, once per-turn healing exists.
        rDefender.ClearOrder();
    }
    return combat;
}

void UnitOrderExecutor::RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval)
{
    FactionRevealedUnits& rRevealed = rMover.GetFaction().GetRevealedUnits();
    for (Unit* pUnit : rEval.blockingUnits)
    {
        rRevealed.Reveal(*pUnit);
    }
}

void UnitOrderExecutor::CollectVisibleHostileIds_(const Unit& rObserver,
                                                  std::unordered_set<UnitId_t>& rOut) const
{
    const Faction& rFaction = rObserver.GetFaction();
    const FactionId_t observerId = rFaction.GetFactionId();

    // TODO: sweep only units whose visibility may have changed; needs a visibility revision.
    m_rWorldMap.GetUnitPositions().ForEachUnit([&](const Unit& rUnit)
    {
        if (rUnit.GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rFaction, rUnit))
        {
            rOut.insert(rUnit.GetUnitId());
        }
    });
}

bool UnitOrderExecutor::HasNewlyVisibleHostile_(
    const Unit& rObserver, const std::unordered_set<UnitId_t>& rPreviouslyVisible) const
{
    std::unordered_set<UnitId_t> nowVisible;
    CollectVisibleHostileIds_(rObserver, nowVisible);
    for (UnitId_t id : nowVisible)
    {
        if (!rPreviouslyVisible.contains(id))
        {
            return true;
        }
    }
    return false;
}

void UnitOrderExecutor::CancelMoveOrderIfNewHostile_(
    Unit& rMover, const std::unordered_set<UnitId_t>& rPreviouslyVisible)
{
    if (!rMover.GetOrder().has_value()
        || !std::holds_alternative<MoveOrder_t>(*rMover.GetOrder()))
    {
        return;
    }
    if (HasNewlyVisibleHostile_(rMover, rPreviouslyVisible))
    {
        rMover.ClearOrder();
    }
}

UnitOrderExecutor::AirdropActionResult_t UnitOrderExecutor::TryAirdrop(Unit& rUnit,
                                                                       const Tile& rDest)
{
    AirdropActionResult_t result;
    const AirdropEligibility_t eligibility = CanAirdropTo(rUnit, rDest);
    if (!eligibility.Ok())
    {
        result.failReason = eligibility.failReason;
        return result;
    }

    rUnit.MarkAirdropped();
    rUnit.ClearOrder();

    const AirdropLanding_t landing = AirdropLanding(rUnit, rDest);
    rUnit.SetCurrentHp(landing.hpAfter);

    EnterTile_(rUnit, rDest);
    result.bEntered = true;
    if (!ApplyArrivalEffects_(rUnit))
    {
        result.bMoverDestroyed = true;
        return result;
    }

    if (landing.bEndsTurn)
    {
        rUnit.SpendRemainingMoveFragments();
    }

    return result;
}

bool UnitOrderExecutor::ApplyArrivalEffects_(Unit& rMover)
{
    // Boards only when the mover cannot hold the tile itself; in a base it stays a garrison.
    ac::TryAutoAttachOnEntry(rMover);

    if (TileHasVisitEffects(rMover.GetTile()))
    {
        // Stop multi-hop so the unit does not walk off before Investigate / Leave resolves.
        rMover.ClearOrder();
        m_rWorld.OnImprovementVisited(rMover);
    }

    // A native raider spends itself on the raid, so this can free rMover.
    return !m_rWorld.ResolveBaseEntryConquest(rMover, m_rRng).bActorDestroyed;
}

void UnitOrderExecutor::EnterTile_(Unit& rMover, const Tile& rTo)
{
    if (rMover.IsEmbarked())
    {
        rMover.Disembark();
    }

    m_rWorldMap.GetUnitPositions().MoveUnit(rMover, rTo);
}

StepResult_t UnitOrderExecutor::SpendMovesAndEnter_(Unit& rMover, const Tile& rTo,
                                                    MoveOrder_t& rMoveOrder)
{
    const EntryTerms_t terms = m_rMoveCosts.ForUnit(rMover, m_rWorldMap).EntryTerms(rTo);
    const int paidSoFar = rMoveOrder.pChargeTile == &rTo ? rMoveOrder.chargeFragmentsPaid : 0;
    const EntryPayment_t payment =
        ResolveEntryPayment(terms, rMover.GetMoveFragmentsRemaining(), paidSoFar);
    rMover.SpendMoveFragments(payment.chargedFragments);
    if (!payment.bEnters)
    {
        rMoveOrder.pChargeTile = &rTo;
        rMoveOrder.chargeFragmentsPaid = payment.paidAfter;
        return {};
    }

    rMoveOrder.pChargeTile = nullptr;
    rMoveOrder.chargeFragmentsPaid = 0;
    EnterTile_(rMover, rTo);

    StepResult_t result;
    result.bEntered = true;
    result.bMoverDestroyed = !ApplyArrivalEffects_(rMover);
    return result;
}

StepResult_t UnitOrderExecutor::TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder)
{
    if (rMover.GetMoveFragmentsRemaining() <= 0)
    {
        return {};
    }

    std::unordered_set<UnitId_t> visibleBefore;
    CollectVisibleHostileIds_(rMover, visibleBefore);

    const StepEvaluation_t eval = m_rSteps.EvaluateStep(rMover, rMover.GetTile(), rTo);
    if (eval.outcome == StepOutcome_t::Legal)
    {
        const StepResult_t result = SpendMovesAndEnter_(rMover, rTo, rMoveOrder);
        if (result.bEntered && !result.bMoverDestroyed)
        {
            CancelMoveOrderIfNewHostile_(rMover, visibleBefore);
        }
        return result;
    }

    if (eval.outcome == StepOutcome_t::BlockedByOccupant
        || eval.outcome == StepOutcome_t::BlockedByZoc)
    {
        RevealBlockingUnits_(rMover, eval);
        CancelMoveOrderIfNewHostile_(rMover, visibleBefore);
    }
    return {};
}

std::optional<CombatResult_t> UnitOrderExecutor::TryAttack(Unit& rAttacker,
                                                           const Tile& rTargetTile)
{
    Unit* pDefender = FindAttackableHostileOnTile(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return std::nullopt;
    }

    // Reported once the attack resolves, so a sneak attack's shared-tile evacuation cannot
    // move either side before the blow lands.
    const HostileAct_t act = HostileActAgainst(rAttacker, *pDefender);
    std::optional<CombatResult_t> result = TryIntercept_(rAttacker, *pDefender);
    if (!result)
    {
        result = FightDefender_(rAttacker, *pDefender);
    }
    ReportHostileAct_(act);
    return result;
}

CombatResult_t UnitOrderExecutor::FightDefender_(Unit& rAttacker, Unit& rDefender)
{
    // Read before Resolve may destroy the defender.
    const Tile& rDefenderTile = rDefender.GetTile();

    std::vector<const Tile*> scrambleHops;
    Unit& rCombatDefender = ResolveScrambleDefender_(rAttacker, rDefender, scrambleHops);

    CombatResult_t result = m_combat.Resolve(rAttacker, rCombatDefender);
    result.scramblePath = std::move(scrambleHops);
    PromoteOnKill_(rAttacker, rCombatDefender, result);

    // Combat never moves either side, so capturing an emptied base is a later step onto it.
    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Attack))
    {
        result.bAttackerDestroyed = true;
    }

    // A native raider spends itself on the raid, so playback must not show it surviving.
    if (result.bDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rDefenderTile))
    {
        result.bAttackerDestroyed = true;
    }
    return result;
}

void UnitOrderExecutor::ReportHostileAct_(const HostileAct_t& rAct)
{
    m_rWorld.OnHostileAct(rAct);
}

std::optional<CombatResult_t> UnitOrderExecutor::TryIntercept_(Unit& rAttacker, Unit& rDefender)
{
    std::optional<CombatResult_t> intercepted =
        m_rWorld.TryInterceptAttack(rAttacker, rDefender, m_rRng);
    if (!intercepted)
    {
        return std::nullopt;
    }
    // A defender lost to its own intercept was never fought: the attack is spent, with no kill
    // to promote and no last defender lost in battle.
    if (!intercepted->bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Attack))
    {
        intercepted->bAttackerDestroyed = true;
    }
    return intercepted;
}

std::optional<UnitOrderExecutor::BombardResult_t> UnitOrderExecutor::TryBombard(
    Unit& rAttacker, const Tile& rTargetTile)
{
    if (!IsWithinBombardRange(rAttacker, rTargetTile))
    {
        return std::nullopt;
    }

    Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTargetTile);
    const std::vector<Unit*> strikeTargets = BombardStrikeTargets(rAttacker, rTargetTile);
    const std::vector<std::string> improvementTargets = BombardImprovementTargets(rTargetTile);
    const std::vector<HostileAct_t> acts = BombardHostileActs(rAttacker, rTargetTile);

    BombardResult_t result;
    result.combats = FireBombard_(rAttacker, pDuelTarget, strikeTargets);
    result.bAttackerDestroyed = !result.combats.empty() && result.combats.back().bAttackerDestroyed;
    if (!improvementTargets.empty())
    {
        result.destroyedImprovementId = DestroyRandomImprovement_(rTargetTile, improvementTargets);
    }

    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Bombard))
    {
        result.bAttackerDestroyed = true;
    }

    const bool bAnyDefenderDestroyed = std::ranges::any_of(
        result.combats, [](const CombatResult_t& rCombat) { return rCombat.bDefenderDestroyed; });
    if (bAnyDefenderDestroyed && !result.bAttackerDestroyed
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

std::vector<CombatResult_t> UnitOrderExecutor::FireBombard_(
    Unit& rAttacker, Unit* pDuelTarget, const std::vector<Unit*>& rStrikeTargets)
{
    std::vector<CombatResult_t> combats;
    if (pDuelTarget)
    {
        combats.push_back(ResolveBombardExchange_(
            rAttacker, *pDuelTarget, CombatEngagement_t::ArtilleryDuel));
    }
    for (Unit* pTarget : rStrikeTargets)
    {
        if (!combats.empty() && combats.back().bAttackerDestroyed)
        {
            break;
        }
        combats.push_back(ResolveBombardExchange_(
            rAttacker, *pTarget, CombatEngagement_t::ArtilleryStrike));
    }
    return combats;
}

std::string UnitOrderExecutor::DestroyRandomImprovement_(
    const Tile& rTile, const std::vector<std::string>& rCandidates)
{
    std::uniform_int_distribution<size_t> pick(0, rCandidates.size() - 1);
    const std::string id = rCandidates[pick(m_rRng)];
    m_rTileEffects.RemoveOccupantWithEffects(MutableTile_(m_rWorldMap, rTile), id);
    return id;
}

BaseManager* UnitOrderExecutor::TryFoundBase(Unit& rUnit, GameState& rGameState)
{
    if (!CanFoundBase(rUnit))
    {
        return nullptr;
    }

    Faction& rFaction = rUnit.GetFaction();
    Tile& rTile = MutableTile_(rGameState.GetWorldMap(), rUnit.GetTile());
    BaseManager* pBase = rFaction.CreateBase(rGameState.AllocateBaseId(),
                                             rFaction.SuggestBaseName(), &rTile,
                                             rGameState.GetTileEffects(), std::nullopt);

    // Before SingleUse destroy: founding-unit StartingMinerals still resolve on the pod.
    ApplyStartingMinerals(*pBase, &rUnit);

    if (ExpendIfSingleUse_(rUnit) == OrderProgress_t::Expended)
    {
        rFaction.GetUnitManager().DestroyUnit(rUnit);
    }
    return pBase;
}

bool UnitOrderExecutor::CanStartTerraformProject(const Unit& rUnit,
                                                 const std::string& projectId) const
{
    const std::optional<TerraformProject_t> resolved =
        FindTerraformProject(projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations);
    return resolved && CanStartTerraform(rUnit, *resolved);
}

bool UnitOrderExecutor::TryStartTerraform(Unit& rUnit, const std::string& projectId,
                                          GameState& rGameState)
{
    if (!CanStartTerraformProject(rUnit, projectId))
    {
        return false;
    }
    const TerraformProject_t resolved =
        *FindTerraformProject(projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations);

    const int cost = TerraformEnergyCost(rUnit, resolved);
    rUnit.GetFaction().GetEconomy().SpendEnergy(cost);
    rUnit.SetOrder(TerraformOrder_t{projectId, resolved.project.turnsRequired});
    rUnit.SpendRemainingMoveFragments();
    return true;
}

OrderProgress_t UnitOrderExecutor::Execute(Unit& rUnit)
{
    if (!rUnit.GetOrder().has_value())
    {
        return OrderProgress_t::Complete;
    }

    // Non-const, since Execute_ counts down an order's turns.
    const OrderProgress_t progress = std::visit([&](auto& rOrder) -> OrderProgress_t
    {
        return Execute_(rUnit, rOrder);
    }, *rUnit.GetOrder());

    // The unit is already gone on UnitDestroyed — there is no order left to clear.
    if (progress != OrderProgress_t::Continue && progress != OrderProgress_t::UnitDestroyed)
    {
        rUnit.ClearOrder();
    }
    return progress;
}

void UnitOrderExecutor::OnTurnStart(Unit& rUnit)
{
    if (rUnit.GetOrder().has_value()
        && std::holds_alternative<SkipTurnOrder_t>(*rUnit.GetOrder()))
    {
        rUnit.ClearOrder();
    }
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, MoveOrder_t& rOrder)
{
    if (!rOrder.pDestination)
    {
        throw std::runtime_error("MoveOrder has null destination");
    }

    // Snapshot: TryStep may ClearOrder on new hostiles, invalidating rOrder.
    const Tile& rDestination = *rOrder.pDestination;
    std::optional<OrderProgress_t> progress;
    while (!progress)
    {
        progress = AdvanceMove_(rUnit, rDestination);
    }
    return *progress;
}

std::optional<OrderProgress_t> UnitOrderExecutor::AdvanceMove_(Unit& rUnit,
                                                              const Tile& rDestination)
{
    if (&rUnit.GetTile() == &rDestination)
    {
        return OrderProgress_t::Complete;
    }

    if (rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return OrderProgress_t::Continue;
    }

    // Re-resolve each step: rOrder from visit is dangling after mid-flight ClearOrder.
    MoveOrder_t& rLiveOrder = std::get<MoveOrder_t>(*rUnit.GetOrder());

    // Path is recalculated every step so newly revealed fog / hostiles are accounted for.
    const Tile* pNext = m_rPathfinder.NextStep(rUnit, rDestination);
    if (!pNext)
    {
        return StepTowardContact_(rUnit, rDestination, rLiveOrder);
    }

    if (m_rSteps.EvaluateStep(rUnit, rUnit.GetTile(), *pNext).outcome
        == StepOutcome_t::BlockedByTerritory)
    {
        return RefuseTerritoryEntry_(rUnit, *pNext);
    }

    const Tile* pTileBefore = &rUnit.GetTile();
    const int movesBefore = rUnit.GetMoveFragmentsRemaining();

    const StepResult_t stepped = TryStep(rUnit, *pNext, rLiveOrder);
    if (stepped.bMoverDestroyed)
    {
        return OrderProgress_t::UnitDestroyed;
    }
    if (!stepped.bEntered)
    {
        return ProgressOfHaltedMove_(rUnit);
    }

    if (&rUnit.GetTile() == pTileBefore && rUnit.GetMoveFragmentsRemaining() == movesBefore)
    {
        return OrderProgress_t::Continue;
    }

    if (!rUnit.GetOrder().has_value())
    {
        return OrderProgress_t::Complete;
    }
    return std::nullopt;
}

OrderProgress_t UnitOrderExecutor::StepTowardContact_(Unit& rUnit, const Tile& rDestination,
                                                      MoveOrder_t& rOrder)
{
    const Tile* pDesired = m_rPathfinder.DesiredContactStep(rUnit, rDestination);
    if (pDesired && TryStep(rUnit, *pDesired, rOrder).bMoverDestroyed)
    {
        return OrderProgress_t::UnitDestroyed;
    }
    return ProgressOfHaltedMove_(rUnit);
}

OrderProgress_t UnitOrderExecutor::RefuseTerritoryEntry_(Unit& rUnit, const Tile& rNext)
{
    if (rUnit.GetFaction().IsPlayerControlled())
    {
        m_rWorld.OnTerritoryEntryRefused(rUnit, m_rWorldMap.GetTerritory().GetOwner(rNext));
        return OrderProgress_t::Continue;
    }
    rUnit.ClearOrder();
    return OrderProgress_t::Complete;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit&, HoldOrder_t&)
{
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit&, HoldUntilHealedOrder_t&)
{
    // TODO: per-turn healing. Repair inside a base must check MayRepairAt (DiplomaticPermissionRules.h).
    // Clear the order when the unit reaches full HP.
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit&, HoldForTurnsOrder_t& rOrder)
{
    if (rOrder.turnsRemaining <= 0)
    {
        return OrderProgress_t::Complete;
    }

    --rOrder.turnsRemaining;
    return rOrder.turnsRemaining == 0 ? OrderProgress_t::Complete
                                      : OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit&, SkipTurnOrder_t&)
{
    // Persist for the rest of this turn (needs-orders exclusion / cancellable). Cleared in
    // OnTurnStart when moves refresh — not here, or a mid-pass Yield would re-prompt.
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit&, SupplyCrawlOrder_t&)
{
    // Harvest happens in ResourceCollection via HomeBaseIndex units in ResourceManager.
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, TerraformOrder_t& rOrder)
{
    if (rOrder.turnsRemaining <= 0)
    {
        return OrderProgress_t::Complete;
    }

    --rOrder.turnsRemaining;
    if (rOrder.turnsRemaining > 0)
    {
        return OrderProgress_t::Continue;
    }

    // Order remains until Execute clears on Complete — safe to read rOrder here.
    const TerraformProject_t project = FindTerraformProject(
        rOrder.projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations).value();
    Tile& rTile = MutableTile_(m_rWorldMap, rUnit.GetTile());

    // Fails only on a surface the project cannot change, such as the wrong depth band.
    if (!ApplyTerraformResult(rTile, project, m_rTileEffects, rUnit, m_rRng, m_rWorld))
    {
        std::cerr << "Terraform completed with no effect: '" << rOrder.projectId
                  << "' could not be applied at (" << rTile.GetX() << ", " << rTile.GetY()
                  << ") — the tile's surface will not take it\n";
    }
    return OrderProgress_t::Complete;
}

Unit& UnitOrderExecutor::ResolveScrambleDefender_(Unit& rAttacker,
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

bool UnitOrderExecutor::ScrambleTo_(Unit& rScrambler, const Tile& rDest,
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

    if (Execute(rScrambler) == OrderProgress_t::UnitDestroyed)
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
