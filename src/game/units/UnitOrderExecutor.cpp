#include "game/units/UnitOrderExecutor.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/movement/Pathfinder.h"
#include "game/movement/StepEvaluator.h"
#include "game/terraform/TerraformMutations.h"
#include "game/terraform/TerraformRules.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/UnitMoveExecutor.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

// A move that stops short goes on next turn, unless contact cleared its order.
OrderProgress_t ProgressOfHaltedMove_(const Unit& rUnit)
{
    return rUnit.GetOrder().has_value() ? OrderProgress_t::Continue : OrderProgress_t::Complete;
}

} // namespace

UnitOrderExecutor::UnitOrderExecutor(UnitMoveExecutor& rMoves, const StepEvaluator& rSteps,
                                     WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                     Pathfinder& rPathfinder,
                                     const TerrainOperationRegistry& rTerrainOperations,
                                     std::mt19937& rRng, IUnitOrderWorld& rWorld)
    : m_rMoves(rMoves)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_rTerrainOperations(rTerrainOperations)
    , m_rRng(rRng)
    , m_rWorld(rWorld)
{
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

    const StepResult_t stepped = m_rMoves.TryStep(rUnit, *pNext, rLiveOrder);
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
    if (pDesired && m_rMoves.TryStep(rUnit, *pDesired, rOrder).bMoverDestroyed)
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
    Tile& rTile = m_rWorldMap.MutableTile(rUnit.GetTile());

    // Fails only on a surface the project cannot change, such as the wrong depth band.
    if (!ApplyTerraformResult(rTile, project, m_rTileEffects, rUnit, m_rRng, m_rWorld))
    {
        std::cerr << "Terraform completed with no effect: '" << rOrder.projectId
                  << "' could not be applied at (" << rTile.GetX() << ", " << rTile.GetY()
                  << ") — the tile's surface will not take it\n";
    }
    return OrderProgress_t::Complete;
}

} // namespace ac
