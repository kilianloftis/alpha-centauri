#include "game/units/UnitOrderExecutor.h"

#include "game/units/InterceptRules.h"
#include "game/units/ScrambleRules.h"
#include "game/units/UnitOrder.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/AttackRules.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/FoundBaseRules.h"
#include "game/units/AirdropRules.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/TransportRules.h"
#include "game/units/TerraformRules.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/EconomyManager.h"
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
#include "game/Faction.h"
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

UnitOrderExecutor::UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                                     const StepEvaluator& rSteps,
                                     WorldMap& rWorldMap,
                                     TileEffectsContext& rTileEffects,
                                     Pathfinder& rPathfinder,
                                     const MoraleCalculator& rMorale,
                                     const TerrainOperationRegistry& rTerrainOperations,
                                     std::mt19937& rRng,
                                     IUnitOrderWorld* pWorld)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_rMorale(rMorale)
    , m_rTerrainOperations(rTerrainOperations)
    , m_rRng(rRng)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, rMorale, rRng)
    , m_pWorld(pWorld)
{
}

OrderProgress_t UnitOrderExecutor::ExpendIfSingleUse_(const Unit& rUnit) const
{
    return rUnit.GetFlag(RuleFlagId_t::SingleUse) ? OrderProgress_t::Expended
                                                  : OrderProgress_t::Complete;
}

void UnitOrderExecutor::PromoteOnKill_(Unit& rAttacker, Unit& rDefender,
                                       const CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed && !rResult.bAttackerDestroyed)
    {
        m_rMorale.TryPromote(rAttacker, rResult.attackStrength, rResult.defenseStrength, m_rRng);
    }
    if (rResult.bAttackerDestroyed && !rResult.bDefenderDestroyed)
    {
        m_rMorale.TryPromote(rDefender, rResult.attackStrength, rResult.defenseStrength, m_rRng);
    }
}

bool UnitOrderExecutor::SpendAttackAction_(Unit& rAttacker, bool bSpendRemaining)
{
    rAttacker.MarkAttacked();
    if (bSpendRemaining)
    {
        rAttacker.SpendRemainingMoveFragments();
    }
    else
    {
        rAttacker.SpendMoveFragments(MovementConstants_t::k_moveFragmentsPerPoint);
    }
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
    if (!m_pWorld)
    {
        return false;
    }
    return m_pWorld->ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, m_rRng)
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
        // TODO: per-turn healing is not implemented. When it is, a unit subjected to
        // bombard must not heal this turn. ClearOrder does not do that by itself.
        rDefender.ClearOrder();
    }
    return combat;
}

void UnitOrderExecutor::RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval)
{
    FactionRevealedUnits& rRevealed = rMover.GetFaction().GetRevealedUnits();
    for (Unit* pUnit : rEval.blockingUnits)
    {
        if (pUnit)
        {
            rRevealed.Reveal(*pUnit);
        }
    }
}

void UnitOrderExecutor::CollectVisibleHostileIds_(const Unit& rObserver,
                                                  std::unordered_set<UnitId_t>& rOut) const
{
    const Faction& rFaction = rObserver.GetFaction();
    const FactionId_t observerId = rFaction.GetFactionId();

    // Sweep the position index, not the map. This runs twice per attempted step (before and
    // after), so a multi-step move used to pay O(steps x tiles x units) — dominated by empty
    // fog on a real map. The index holds only occupied tiles, making it O(units) per sweep.
    //
    // TODO: still O(units) per step. Restricting the sweep to units whose visibility could have
    // changed (a dirty set invalidated by reveal/move) would make it proportional to what
    // actually moved, but needs a visibility-revision seam that does not exist yet.
    m_rWorldMap.GetUnitPositions().ForEachUnit([&](const Unit& rUnit)
    {
        if (rUnit.GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rFaction, rUnit, m_rTileEffects))
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

Unit* UnitOrderExecutor::FindVisibleHostileOnTile(const Unit& rObserver,
                                                  const Tile& rTile) const
{
    return ac::FindVisibleHostileOnTile(rObserver, rTile, m_rWorldMap, m_rTileEffects);
}

bool UnitOrderExecutor::TryAttachToTransport(Unit& rPassenger)
{
    return ac::TryAttachToTransport(rPassenger, m_rWorldMap);
}

bool UnitOrderExecutor::TryAutoAttachWhenMustLand(Unit& rPassenger)
{
    return ac::TryAutoAttachWhenMustLand(rPassenger, m_rWorldMap);
}

bool UnitOrderExecutor::TryUnloadTransport(Unit& rCarrier)
{
    return TryUnloadTransportInPlace(rCarrier, m_rWorldMap,
                                     m_rTileEffects.GetInteractionGrids());
}

UnitOrderExecutor::AirdropActionResult_t UnitOrderExecutor::TryAirdrop(Unit& rUnit,
                                                                       const Tile& rDest)
{
    AirdropActionResult_t result;
    const AirdropEligibility_t eligibility = CanAirdropTo(rUnit, rDest, m_rWorldMap, m_rTileEffects);
    if (!eligibility.Ok())
    {
        result.failReason = eligibility.failReason;
        return result;
    }

    rUnit.MarkAirdropped();
    rUnit.ClearOrder();

    const int damageHp = AirdropLandingDamageHp(rUnit, rDest, m_rWorldMap);
    if (damageHp > 0)
    {
        const int hp = rUnit.GetCurrentHp();
        rUnit.SetCurrentHp(std::max(1, hp - damageHp));
    }

    EnterTile_(rUnit, rDest);
    result.bEntered = true;
    if (!ApplyArrivalEffects_(rUnit))
    {
        result.bMoverDestroyed = true;
        return result;
    }

    if (!rUnit.IsCombatUnit())
    {
        rUnit.SpendRemainingMoveFragments();
    }

    return result;
}

bool UnitOrderExecutor::ApplyArrivalEffects_(Unit& rMover)
{
    // Board a transport on this tile only when the mover cannot hold the tile itself
    // (step onto open water / ship-to-ship transfer); entering a base leaves it a garrison,
    // not cargo.
    ac::TryAutoAttachOnEntry(rMover, m_rWorldMap, m_rTileEffects.GetInteractionGrids());

    if (TileHasVisitEffects(rMover.GetTile()))
    {
        // Stop multi-hop so the unit does not walk off before Investigate / Leave resolves.
        rMover.ClearOrder();

        if (m_improvementVisitHandler)
        {
            m_improvementVisitHandler(rMover);
        }
    }

    // No world bound means no session to conquer into — a legitimate mode for movement-only
    // harnesses.
    if (!m_pWorld)
    {
        return true;
    }
    // A native raider spends itself on the raid, so this can free rMover.
    return !m_pWorld->ResolveBaseEntryConquest(rMover, m_rRng).bActorDestroyed;
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
    const int available = rMover.GetMoveFragmentsRemaining();

    if (terms.bRequiresFullCost)
    {
        if (rMoveOrder.pChargeTile != &rTo)
        {
            rMoveOrder.pChargeTile = &rTo;
            rMoveOrder.chargeFragmentsPaid = 0;
        }
        if (rMoveOrder.chargeFragmentsPaid + available < terms.costFragments)
        {
            // Bank this turn's fragments toward the entry price and stay put.
            rMoveOrder.chargeFragmentsPaid += available;
            rMover.SpendRemainingMoveFragments();
            return {};
        }
    }

    rMoveOrder.pChargeTile = nullptr;
    rMoveOrder.chargeFragmentsPaid = 0;

    // Standard terrain admits any positive balance (SpendMoveFragments clamps a negative
    // result to 0); end-turn entries spend whatever remains.
    if (terms.bEndsTurn)
    {
        rMover.SpendRemainingMoveFragments();
    }
    else
    {
        rMover.SpendMoveFragments(terms.costFragments);
    }
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
    Unit* pDefender =
        FindAttackableHostileOnTile(rAttacker, rTargetTile, m_rWorldMap, m_rTileEffects);
    if (!pDefender)
    {
        return std::nullopt;
    }

    // Reported once the attack resolves, so a sneak attack's shared-tile evacuation cannot
    // move either side before the blow lands.
    const HostileAct_t act = HostileActAgainst(rAttacker, *pDefender);
    if (m_pWorld)
    {
        if (std::optional<CombatResult_t> intercepted =
                m_pWorld->TryInterceptAttack(rAttacker, *pDefender, m_rTileEffects, m_rRng))
        {
            // Intercept destroys the attacker; no attack history / move spend on a dead unit.
            m_pWorld->OnHostileAct(act);
            return intercepted;
        }
    }

    // Snapshot before Resolve may DestroyUnit the defender (and free its tile reference).
    const Tile& rDefenderTile = pDefender->GetTile();
    const bool bDefenderOnBase =
        m_pWorld && m_pWorld->FindBaseAt(rDefenderTile.GetX(), rDefenderTile.GetY()) != nullptr;

    std::vector<const Tile*> scrambleHops;
    Unit& rCombatDefender = ResolveScrambleDefender_(rAttacker, *pDefender, scrambleHops);

    CombatResult_t result = m_combat.Resolve(rAttacker, rCombatDefender);
    result.scramblePath = std::move(scrambleHops);
    PromoteOnKill_(rAttacker, rCombatDefender, result);

    if (!result.bAttackerDestroyed)
    {
        // Combat never moves either side. Default cost is one movement point (and one fuel
        // point when the design tracks fuel). AttackingEndsTurn (Needlejet) spends the rest
        // of the turn's moves instead. Capture of an emptied base is a later step onto the
        // tile while moves remain.
        const bool bSpendRemaining = rAttacker.GetFlag(RuleFlagId_t::AttackingEndsTurn);
        if (SpendAttackAction_(rAttacker, bSpendRemaining))
        {
            result.bAttackerDestroyed = true;
        }
    }

    // Last-defender casualties / adjacent native raid — not ownership transfer. Capture
    // requires a separate enter-tile order after combat. A native raider spends itself on
    // the raid; report that so UI playback does not show a survivor that no longer exists.
    if (result.bDefenderDestroyed && !result.bAttackerDestroyed && bDefenderOnBase
        && ApplyLastDefenderConquest_(rAttacker, rDefenderTile))
    {
        result.bAttackerDestroyed = true;
    }
    if (m_pWorld)
    {
        m_pWorld->OnHostileAct(act);
    }
    return result;
}

std::vector<HostileAct_t> UnitOrderExecutor::BombardHostileActs_(
    const Unit& rAttacker, const Tile& rTargetTile, const BombardTargeting_t& rTargeting,
    bool bOccupied) const
{
    std::vector<HostileAct_t> acts;
    const auto addAct = [&](const HostileAct_t& rAct)
    {
        if (rAct.victim == rAct.aggressor || rAct.victim == k_NoFactionOwner)
        {
            return;
        }
        const auto it = std::find_if(acts.begin(), acts.end(), [&](const HostileAct_t& rKnown)
                                     { return rKnown.victim == rAct.victim; });
        if (it == acts.end())
        {
            acts.push_back(rAct);
        }
        else
        {
            it->bAttributed = it->bAttributed || rAct.bAttributed;
        }
    };
    if (rTargeting.pDuelTarget)
    {
        addAct(HostileActAgainst(rAttacker, *rTargeting.pDuelTarget));
    }
    else if (!rTargeting.bBombardPresentButIllegal)
    {
        for (const Unit* pTarget : rTargeting.strikeTargets)
        {
            if (pTarget)
            {
                addAct(HostileActAgainst(rAttacker, *pTarget));
            }
        }
    }
    if (!bOccupied && !NonBaseImprovementIds(rTargetTile).empty())
    {
        addAct(HostileActAgainst(rAttacker, m_rWorldMap.GetTerritory().GetOwner(rTargetTile)));
    }
    return acts;
}

std::optional<UnitOrderExecutor::BombardResult_t> UnitOrderExecutor::TryBombard(
    Unit& rAttacker, const Tile& rTargetTile)
{
    if (!IsWithinBombardRange(rAttacker, rTargetTile, m_rWorldMap))
    {
        return std::nullopt;
    }

    const bool bOccupied = TileHasUnits(rTargetTile, m_rWorldMap);
    BombardResult_t result;
    const BombardTargeting_t targeting =
        CollectBombardTargets(rAttacker, rTargetTile, m_rWorldMap, m_rTileEffects);
    const std::vector<HostileAct_t> acts =
        BombardHostileActs_(rAttacker, rTargetTile, targeting, bOccupied);

    if (targeting.pDuelTarget)
    {
        result.combats.push_back(ResolveBombardExchange_(
            rAttacker, *targeting.pDuelTarget, CombatEngagement_t::ArtilleryDuel));
        result.bAttackerDestroyed = result.combats.back().bAttackerDestroyed;
    }
    else if (!targeting.bBombardPresentButIllegal)
    {
        for (Unit* pTarget : targeting.strikeTargets)
        {
            if (!pTarget || result.bAttackerDestroyed)
            {
                break;
            }
            result.combats.push_back(ResolveBombardExchange_(
                rAttacker, *pTarget, CombatEngagement_t::ArtilleryStrike));
            result.bAttackerDestroyed = result.combats.back().bAttackerDestroyed;
        }
    }

    if (!bOccupied)
    {
        const std::vector<std::string> candidates = NonBaseImprovementIds(rTargetTile);
        if (!candidates.empty())
        {
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            const std::string id = candidates[pick(m_rRng)];
            if (Tile* pTile = m_rWorldMap.GetTile(rTargetTile.GetX(), rTargetTile.GetY()))
            {
                m_rTileEffects.RemoveOccupantWithEffects(*pTile, id);
                result.destroyedImprovementId = id;
            }
        }
    }

    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, true))
    {
        result.bAttackerDestroyed = true;
    }

    bool bAnyDefenderDestroyed = false;
    for (const CombatResult_t& rCombat : result.combats)
    {
        bAnyDefenderDestroyed = bAnyDefenderDestroyed || rCombat.bDefenderDestroyed;
    }
    if (bAnyDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rTargetTile))
    {
        result.bAttackerDestroyed = true;
        result.combats.back().bAttackerDestroyed = true;
    }
    if (m_pWorld)
    {
        for (const HostileAct_t& rAct : acts)
        {
            m_pWorld->OnHostileAct(rAct);
        }
    }
    return result;
}

BaseManager* UnitOrderExecutor::TryFoundBase(Unit& rUnit, GameState& rGameState)
{
    if (!rUnit.GetFlag(RuleFlagId_t::FoundBase))
    {
        return nullptr;
    }

    Faction& rFaction = rUnit.GetFaction();
    const Tile& rUnitTile = rUnit.GetTile();
    if (rUnitTile.IsWater() && rUnit.GetDomain() != UnitDomain_t::Sea)
    {
        return nullptr;
    }
    if (!CanFoundBaseAt(rUnitTile, rFaction.GetFactionId(), rGameState))
    {
        return nullptr;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnitTile.GetX(), rUnitTile.GetY());
    if (!pTile)
    {
        return nullptr;
    }

    const bool bMayOccupyWater =
        rUnitTile.IsWater() && rUnit.GetDomain() == UnitDomain_t::Sea;
    BaseManager* pBase = rFaction.CreateBase(
        rGameState.AllocateBaseId(),
        rFaction.SuggestBaseName(),
        pTile,
        rGameState.GetTileEffects(),
        rGameState.GetSecretProjectAvailability(),
        std::nullopt,
        bMayOccupyWater);

    // Before SingleUse destroy: founding-unit StartingMinerals still resolve on the pod.
    ApplyStartingMinerals(*pBase, &rUnit);

    if (ExpendIfSingleUse_(rUnit) == OrderProgress_t::Expended)
    {
        rFaction.GetUnitManager().DestroyUnit(rUnit);
    }
    return pBase;
}

bool UnitOrderExecutor::CanStartTerraformProject(const Unit& rUnit, const std::string& projectId,
                                                 const GameState& rGameState) const
{
    const std::optional<TerraformProject_t> resolved =
        FindTerraformProject(projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations);
    return resolved && CanStartTerraform(rUnit, *resolved, rGameState);
}

bool UnitOrderExecutor::TryStartTerraform(Unit& rUnit, const std::string& projectId,
                                          GameState& rGameState)
{
    if (!CanStartTerraformProject(rUnit, projectId, rGameState))
    {
        return false;
    }
    const TerraformProject_t resolved =
        *FindTerraformProject(projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations);

    const int cost = TerraformEnergyCost(rUnit, resolved, rGameState);
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

    // non-const visit — allows mutating HoldForTurnsOrder_t / TerraformOrder_t
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
        throw std::runtime_error("MoveOrder has null destination");

    // Snapshot: TryStep may ClearOrder on new hostiles, invalidating rOrder.
    const Tile* const pDestination = rOrder.pDestination;

    while (true)
    {
        if (&rUnit.GetTile() == pDestination)
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
        const Tile* pNext = m_rPathfinder.NextStep(rUnit, *pDestination);
        if (!pNext)
        {
            const Tile* pDesired = m_rPathfinder.DesiredContactStep(rUnit, *pDestination);
            if (pDesired && TryStep(rUnit, *pDesired, rLiveOrder).bMoverDestroyed)
            {
                return OrderProgress_t::UnitDestroyed;
            }
            // Order may have been cleared by contact; otherwise keep it for next turn.
            return rUnit.GetOrder().has_value() ? OrderProgress_t::Continue
                                                : OrderProgress_t::Complete;
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
            return rUnit.GetOrder().has_value() ? OrderProgress_t::Continue
                                                : OrderProgress_t::Complete;
        }

        if (&rUnit.GetTile() == pTileBefore && rUnit.GetMoveFragmentsRemaining() == movesBefore)
        {
            return OrderProgress_t::Continue;
        }

        if (!rUnit.GetOrder().has_value())
        {
            return OrderProgress_t::Complete;
        }
    }
}

OrderProgress_t UnitOrderExecutor::RefuseTerritoryEntry_(Unit& rUnit, const Tile& rNext)
{
    if (m_pWorld && rUnit.GetFaction().IsPlayerControlled())
    {
        m_pWorld->OnTerritoryEntryRefused(rUnit, m_rWorldMap.GetTerritory().GetOwner(rNext));
        return OrderProgress_t::Continue;
    }
    rUnit.ClearOrder();
    return OrderProgress_t::Complete;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, HoldOrder_t& rOrder)
{
    // Hold indefinitely — nothing to do each turn
    (void)rUnit;
    (void)rOrder;
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder)
{
    // TODO: per-turn healing. Repair inside a base must check MayRepairAt (DiplomaticPermissionRules.h).
    // Clear the order when the unit reaches full HP.
    (void)rUnit;
    (void)rOrder;
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder)
{
    (void)rUnit;
    if (rOrder.turnsRemaining <= 0)
    {
        return OrderProgress_t::Complete;
    }

    --rOrder.turnsRemaining;
    return rOrder.turnsRemaining == 0 ? OrderProgress_t::Complete
                                      : OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, SkipTurnOrder_t& rOrder)
{
    // Persist for the rest of this turn (needs-orders exclusion / cancellable). Cleared in
    // OnTurnStart when moves refresh — not here, or a mid-pass Yield would re-prompt.
    (void)rUnit;
    (void)rOrder;
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, SupplyCrawlOrder_t& rOrder)
{
    // Harvest happens in ResourceCollection via HomeBaseIndex units in ResourceManager.
    (void)rUnit;
    (void)rOrder;
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

    // Energy was already debited and the turns are spent. Place clears features that cannot
    // share the tile, then adds its improvement, so an incompatible feature is not a failed
    // completion. A false return is a surface the order does not change (a depth band, or the
    // tile left the improvement's domain).

    // Order remains until Execute clears on Complete — safe to read rOrder here.
    const std::optional<TerraformProject_t> project = FindTerraformProject(
        rOrder.projectId, m_rTileEffects.GetImprovements(), m_rTerrainOperations);
    if (!project)
    {
        std::cerr << "Terraform completed with no effect: project '" << rOrder.projectId
                  << "' is not in the registry\n";
        return OrderProgress_t::Complete;
    }

    Tile* pTile = m_rWorldMap.GetTile(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    if (!pTile)
    {
        std::cerr << "Terraform completed with no effect: unit is not on a valid tile\n";
        return OrderProgress_t::Complete;
    }

    if (!ApplyTerraformResult(*pTile, *project, m_rTileEffects, rUnit, m_rRng))
    {
        std::cerr << "Terraform completed with no effect: '" << rOrder.projectId
                  << "' could not be applied at (" << pTile->GetX() << ", " << pTile->GetY()
                  << ") — the tile's surface will not take it\n";
    }
    return OrderProgress_t::Complete;
}

Unit& UnitOrderExecutor::ResolveScrambleDefender_(Unit& rAttacker,
                                                  Unit& rOriginalDefender,
                                                  std::vector<const Tile*>& rOutPath)
{
    rOutPath.clear();
    Unit* pScrambler = FindScrambler(
        rAttacker, rOriginalDefender, m_rWorldMap, m_rTileEffects, m_rPathfinder);
    if (!pScrambler)
    {
        return rOriginalDefender;
    }

    const Tile& rDest = rOriginalDefender.GetTile();
    MoveOrder_t order;
    order.pDestination = &rDest;
    pScrambler->SetOrder(order);

    auto movedConn = m_rWorldMap.GetUnitPositions().OnUnitMoved.ConnectScoped(
        [&](Unit& rUnit)
        {
            if (&rUnit == pScrambler)
            {
                rOutPath.push_back(&rUnit.GetTile());
            }
        });

    const OrderProgress_t progress = Execute(*pScrambler);
    if (progress == OrderProgress_t::UnitDestroyed)
    {
        rOutPath.clear();
        return rOriginalDefender;
    }
    if (&pScrambler->GetTile() != &rDest)
    {
        rOutPath.clear();
        pScrambler->ClearOrder();
        return rOriginalDefender;
    }
    return *pScrambler;
}

} // namespace ac
