#include "game/units/UnitOrderExecutor.h"

#include "game/units/InterceptRules.h"
#include "game/units/UnitOrder.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/FoundBaseRules.h"
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
#include "game/effects/TileEffectsContext.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
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
                                     std::mt19937& rRng,
                                     IUnitOrderWorld* pWorld)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_rMorale(rMorale)
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

    for (const auto& pTile : m_rWorldMap.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        for (const Unit* pUnit : m_rWorldMap.GetUnitsOnTile(*pTile))
        {
            if (pUnit && pUnit->GetFaction().GetFactionId() != observerId
                && IsUnitVisibleTo(rFaction, *pUnit, m_rTileEffects))
            {
                rOut.insert(pUnit->GetUnitId());
            }
        }
    }
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
    const Faction& rObserverFaction = rObserver.GetFaction();
    const FactionId_t observerId = rObserverFaction.GetFactionId();
    for (Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && !pUnit->IsEmbarked()
            && pUnit->GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rObserverFaction, *pUnit, m_rTileEffects))
        {
            return pUnit;
        }
    }
    return nullptr;
}

bool UnitOrderExecutor::TryAttachToTransport(Unit& rPassenger, bool bRefuelOnAttach)
{
    return ac::TryAttachToTransport(rPassenger, m_rWorldMap, bRefuelOnAttach);
}

bool UnitOrderExecutor::TryAutoAttachWhenMustLand(Unit& rPassenger)
{
    return ac::TryAutoAttachWhenMustLand(rPassenger, m_rWorldMap);
}

bool UnitOrderExecutor::TryUnloadTransport(Unit& rCarrier)
{
    return TryUnloadTransportInPlace(rCarrier);
}

bool UnitOrderExecutor::ApplyArrivalEffects_(Unit& rMover, bool bWasEmbarked)
{
    // Board a transport on this tile only when the mover cannot hold the tile itself
    // (step onto open water); entering a base leaves it a garrison, not cargo.
    if (!bWasEmbarked)
    {
        ac::TryAutoAttachOnEntry(rMover, m_rWorldMap);
    }

    if (!m_pWorld || !m_pGameData)
    {
        return true;
    }
    // A native raider spends itself on the raid, so this can free rMover.
    return !m_pWorld->ResolveBaseEntryConquest(rMover, *m_pGameData, m_rRng).bActorDestroyed;
}

void UnitOrderExecutor::EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter)
{
    if (rMover.IsEmbarked())
    {
        rMover.Disembark();
    }

    m_rWorldMap.GetUnitPositions().MoveUnit(rMover, rTo);

    // Hostile entry into a Bunker spends all remaining moves (capture).
    int remaining = remainingAfter;
    if (rTo.HasImprovement("Bunker"))
    {
        const FactionId_t territoryOwner = m_rWorldMap.GetTerritory().GetOwner(rTo);
        if (territoryOwner != k_NoFactionOwner
            && territoryOwner != rMover.GetFaction().GetFactionId())
        {
            remaining = 0;
        }
    }

    rMover.SetMoveFragmentsRemaining(remaining);
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
            rMover.SetMoveFragmentsRemaining(0);
            return {};
        }
    }

    rMoveOrder.pChargeTile = nullptr;
    rMoveOrder.chargeFragmentsPaid = 0;

    // Standard terrain admits any positive balance (SetMoveFragmentsRemaining clamps a
    // negative result to 0); end-turn entries wipe whatever would remain.
    const int remainingAfter = terms.bEndsTurn ? 0 : available - terms.costFragments;
    const bool bWasEmbarked = rMover.IsEmbarked();
    EnterTile_(rMover, rTo, remainingAfter);

    StepResult_t result;
    result.bEntered = true;
    result.bMoverDestroyed = !ApplyArrivalEffects_(rMover, bWasEmbarked);
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
    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return std::nullopt;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, m_rWorldMap.GetWidth()))
    {
        return std::nullopt;
    }

    Unit* pDefender = FindVisibleHostileOnTile(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return std::nullopt;
    }
    // Land units need Amphibious to attack a garrisoned sea base.
    if (!CanAttackIntoBaseTile(rAttacker, rTargetTile))
    {
        return std::nullopt;
    }

    if (m_pWorld)
    {
        if (std::optional<CombatResult_t> intercepted =
                m_pWorld->TryInterceptAttack(rAttacker, *pDefender, m_rTileEffects, m_rRng))
        {
            // Intercept destroys the attacker; no attack history / move spend on a dead unit.
            return intercepted;
        }
    }

    // Snapshot before Resolve may DestroyUnit the defender (and free its tile reference).
    const Tile& rDefenderTile = pDefender->GetTile();
    const bool bDefenderOnBase =
        m_pWorld && m_pWorld->FindBaseAt(rDefenderTile.GetX(), rDefenderTile.GetY()) != nullptr;

    CombatResult_t result = m_combat.Resolve(rAttacker, *pDefender);

    // Promotion on kill (not mere disengage). Probe teams skipped inside TryPromote.
    if (result.bDefenderDestroyed && !result.bAttackerDestroyed)
    {
        m_rMorale.TryPromote(rAttacker, result.attackStrength, result.defenseStrength, m_rRng);
    }
    if (result.bAttackerDestroyed && !result.bDefenderDestroyed)
    {
        m_rMorale.TryPromote(*pDefender, result.attackStrength, result.defenseStrength, m_rRng);
    }

    if (!result.bAttackerDestroyed)
    {
        rAttacker.MarkAttacked();
        rAttacker.SetMoveFragmentsRemaining(0);
        rAttacker.ClearOrder();
        if (ExpendIfSingleUse_(rAttacker) == OrderProgress_t::Expended)
        {
            rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
            result.bAttackerDestroyed = true;
        }
    }

    // After SingleUse expenditure: only a surviving capturer/raider may take the base.
    if (result.bDefenderDestroyed && !result.bAttackerDestroyed && bDefenderOnBase && m_pWorld
        && m_pGameData)
    {
        // A native raider spends itself on the raid; report that so UI playback does not
        // show a survivor that no longer exists.
        result.bAttackerDestroyed =
            m_pWorld->ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, *m_pGameData, m_rRng)
                .bActorDestroyed;
    }
    return result;
}

BaseManager* UnitOrderExecutor::TryFoundBase(Unit& rUnit, GameState& rGameState,
                                             const GameDataContext& rDataContext,
                                             std::function<void(BaseManager&)> onBaseCreated)
{
    if (!rUnit.GetFlag(RuleFlagId_t::FoundBase))
    {
        return nullptr;
    }

    Faction& rFaction = rUnit.GetFaction();
    const Tile& rUnitTile = rUnit.GetTile();
    if (!CanFoundBaseAt(rUnitTile, rFaction.GetFactionId(), rGameState))
    {
        return nullptr;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnitTile.GetX(), rUnitTile.GetY());
    if (!pTile)
    {
        return nullptr;
    }

    BaseManager* pBase = rFaction.CreateBase(
        rGameState.AllocateBaseId(),
        rFaction.SuggestBaseName(),
        pTile,
        rDataContext,
        rGameState.GetTileEffects(),
        rGameState.GetSecretProjectAvailability());

    if (onBaseCreated)
    {
        onBaseCreated(*pBase);
    }

    if (ExpendIfSingleUse_(rUnit) == OrderProgress_t::Expended)
    {
        rFaction.GetUnitManager().DestroyUnit(rUnit);
    }
    return pBase;
}

bool UnitOrderExecutor::TryStartTerraform(Unit& rUnit, const std::string& improvementId,
                                          GameState& rGameState)
{
    const ImprovementConfig_t* pConfig = m_rTileEffects.GetImprovements().Find(improvementId);
    if (!pConfig)
    {
        return false;
    }
    if (!CanStartTerraform(rUnit, *pConfig, rGameState))
    {
        return false;
    }

    const int cost = TerraformEnergyCost(rUnit, *pConfig, rGameState);
    rUnit.GetFaction().GetEconomy().AddEnergy(-cost);
    rUnit.SetOrder(TerraformOrder_t{improvementId, pConfig->turnsRequired});
    rUnit.SetMoveFragmentsRemaining(0);
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

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, HoldOrder_t& rOrder)
{
    // Hold indefinitely — nothing to do each turn
    (void)rUnit;
    (void)rOrder;
    return OrderProgress_t::Continue;
}

OrderProgress_t UnitOrderExecutor::Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder)
{
    // TODO: Clear order when unit reaches full HP
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

    // Order remains until Execute clears on Complete — safe to read rOrder here.
    const ImprovementConfig_t* pConfig = m_rTileEffects.GetImprovements().Find(rOrder.improvementId);
    if (!pConfig)
    {
        return OrderProgress_t::Complete;
    }

    Tile* pTile = m_rWorldMap.GetTile(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    if (!pTile)
    {
        return OrderProgress_t::Complete;
    }

    ApplyTerraformResult(*pTile, *pConfig, m_rTileEffects, m_rWorldMap, rUnit);
    return OrderProgress_t::Complete;
}

} // namespace ac
