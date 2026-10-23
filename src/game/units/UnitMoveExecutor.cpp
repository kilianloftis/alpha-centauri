#include "game/units/UnitMoveExecutor.h"

#include "game/effects/TriggeredEffectRules.h"
#include "game/faction/Faction.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportMutations.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/visibility/FactionRevealedUnits.h"
#include "game/visibility/VisibilityRules.h"

#include <variant>

namespace ac
{

UnitMoveExecutor::UnitMoveExecutor(const MoveCostRules& rMoveCosts, const StepEvaluator& rSteps,
                                   WorldMap& rWorldMap, std::mt19937& rRng,
                                   IUnitOrderWorld& rWorld)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rRng(rRng)
    , m_rWorld(rWorld)
{
}

void UnitMoveExecutor::RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval)
{
    FactionRevealedUnits& rRevealed = rMover.GetFaction().GetRevealedUnits();
    for (Unit* pUnit : rEval.blockingUnits)
    {
        rRevealed.Reveal(*pUnit);
    }
}

void UnitMoveExecutor::CollectVisibleHostileIds_(const Unit& rObserver,
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

bool UnitMoveExecutor::HasNewlyVisibleHostile_(
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

void UnitMoveExecutor::CancelMoveOrderIfNewHostile_(
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

UnitMoveExecutor::AirdropActionResult_t UnitMoveExecutor::TryAirdrop(Unit& rUnit,
                                                                       const Tile& rDest)
{
    AirdropActionResult_t result;
    const AirdropEligibility_t eligibility = CanAirdropTo(rUnit, rDest);
    if (!eligibility.Ok())
    {
        result.failReason = eligibility.failReason;
        return result;
    }

    rUnit.Latches().MarkAirdropped();
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

bool UnitMoveExecutor::ApplyArrivalEffects_(Unit& rMover)
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

void UnitMoveExecutor::EnterTile_(Unit& rMover, const Tile& rTo)
{
    if (rMover.Transport().IsEmbarked())
    {
        rMover.Transport().Disembark();
    }

    m_rWorldMap.GetUnitPositions().MoveUnit(rMover, rTo);
}

StepResult_t UnitMoveExecutor::SpendMovesAndEnter_(Unit& rMover, const Tile& rTo,
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

StepResult_t UnitMoveExecutor::TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder)
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

} // namespace ac
