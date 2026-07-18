#pragma once

#include "game/units/CombatResolver.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

namespace ac
{

class MoveCostCalculator;
class WorldMap;
class Tile;
class Pathfinder;
struct MoveOrder_t;
struct HoldOrder_t;
struct HoldUntilHealedOrder_t;
struct HoldForTurnsOrder_t;

// Applies step/attack/order actions using shared MoveCostCalculator, StepEvaluator, and
// Pathfinder references. Path search and step legality are shared world queries owned by
// GameState; this class owns only the mutation logic.
class UnitOrderExecutor
{
public:
    UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                      const StepEvaluator& rSteps,
                      WorldMap& rWorldMap,
                      const TileEffectsContext& rTileEffects,
                      Pathfinder& rPathfinder);
    // Same as above, but seeds CombatResolver for deterministic tests.
    UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                      const StepEvaluator& rSteps,
                      WorldMap& rWorldMap,
                      const TileEffectsContext& rTileEffects,
                      Pathfinder& rPathfinder,
                      uint32_t combatSeed);
    ~UnitOrderExecutor() = default;

    void Execute(Unit& rUnit);

    // Apply one legal empty-tile step (MoveUnit + spend tile move-cost fragments). On
    // BlockedByOccupant / BlockedByZoc, contact-reveals the attributed blocking units.
    // Newly revealed hostiles (contact or fog) cancel a pending MoveOrder on rMover.
    // Fungus multi-turn charge progress is stored on rMoveOrder.
    bool TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Attack a hostile-occupied adjacent tile without moving onto it. Returns nullopt if not
    // adjacent, out of moves, or rTargetTile has no hostile unit visible to the attacker.
    // Otherwise resolves combat (HP / DestroyUnit) and returns the round-by-round result for
    // UI playback.
    std::optional<CombatResult_t> TryAttack(Unit& rAttacker, const Tile& rTargetTile);

private:
    void Execute_(Unit& rUnit, MoveOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder);

    Unit* FindVisibleHostileOnTile_(const Unit& rAttacker, const Tile& rTile) const;
    void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval);
    void EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter);
    bool SpendMovesAndEnter_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
    void CollectVisibleHostileIds_(const Unit& rObserver,
                                   std::unordered_set<UnitId_t>& rOut) const;
    bool HasNewlyVisibleHostile_(const Unit& rObserver,
                                 const std::unordered_set<UnitId_t>& rPreviouslyVisible) const;
    void CancelMoveOrderIfNewHostile_(Unit& rMover,
                                      const std::unordered_set<UnitId_t>& rPreviouslyVisible);

    const MoveCostCalculator& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
    Pathfinder& m_rPathfinder;
    CombatResolver m_combat;
};

} // namespace ac
