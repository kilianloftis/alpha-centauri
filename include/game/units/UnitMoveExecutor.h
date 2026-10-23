#pragma once

#include "game/movement/AirdropRules.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <random>
#include <unordered_set>

namespace ac
{

class IUnitOrderWorld;
class MoveCostRules;
class Tile;
class WorldMap;

// A native raider is consumed by the base it raids, so a step can end with the mover gone.
struct StepResult_t
{
    bool bEntered = false;
    bool bMoverDestroyed = false;
};

// Moves a unit one step or one drop: pays for entry, enters, and applies arrival effects.
class UnitMoveExecutor
{
public:
    UnitMoveExecutor(const MoveCostRules& rMoveCosts, const StepEvaluator& rSteps,
                     WorldMap& rWorldMap, std::mt19937& rRng, IUnitOrderWorld& rWorld);

    // One step onto rTo. A blocked step reveals its blockers, and a newly seen hostile cancels
    // a move order. Multi-turn fungus entry progress is kept on rMoveOrder.
    [[nodiscard]] StepResult_t TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Drops rUnit onto rDest with landing damage. failReason says why it could not.
    struct AirdropActionResult_t
    {
        AirdropFailReason_t failReason = AirdropFailReason_t::None;
        bool bEntered = false;
        bool bMoverDestroyed = false;
        bool Ok() const { return failReason == AirdropFailReason_t::None && bEntered; }
    };
    [[nodiscard]] AirdropActionResult_t TryAirdrop(Unit& rUnit, const Tile& rDest);

private:
    // Position only; caller spends moves (SpendMoveFragments) before enter.
    void EnterTile_(Unit& rMover, const Tile& rTo);
    // Returns false when the arrival destroyed rMover (native raid).
    bool ApplyArrivalEffects_(Unit& rMover);
    StepResult_t SpendMovesAndEnter_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
    void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval);
    void CollectVisibleHostileIds_(const Unit& rObserver,
                                   std::unordered_set<UnitId_t>& rOut) const;
    bool HasNewlyVisibleHostile_(const Unit& rObserver,
                                 const std::unordered_set<UnitId_t>& rPreviouslyVisible) const;
    void CancelMoveOrderIfNewHostile_(Unit& rMover,
                                      const std::unordered_set<UnitId_t>& rPreviouslyVisible);

    const MoveCostRules& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
    std::mt19937& m_rRng;
    IUnitOrderWorld& m_rWorld;
};

} // namespace ac
