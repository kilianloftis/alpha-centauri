#pragma once

#include "game/units/StepEvaluator.h"

namespace ac
{

class Unit;
class WorldMap;
class Tile;
class ImprovementRegistry;
class TileEffectsContext;
struct MoveOrder_t;
struct HoldOrder_t;
struct HoldUntilHealedOrder_t;
struct HoldForTurnsOrder_t;

// Owns StepEvaluator and applies mutating step/attack/order actions on top of it.
class UnitOrderExecutor
{
public:
    UnitOrderExecutor(const ImprovementRegistry& rImprovements,
                      WorldMap& rWorldMap,
                      const TileEffectsContext& rTileEffects);
    ~UnitOrderExecutor() = default;

    void Execute(Unit& rUnit);

    // Apply one legal empty-tile step (MoveUnit + spend tile move-cost fragments). On
    // BlockedByOccupant / BlockedByZoc, contact-reveals the attributed blocking units.
    // Fungus multi-turn charge progress is stored on rMoveOrder.
    bool TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Attack a hostile-occupied adjacent tile without moving onto it. Returns false if not
    // adjacent, out of moves, or rTargetTile has no hostile unit visible to the attacker.
    bool TryAttack(Unit& rAttacker, const Tile& rTargetTile);

private:
    void Execute_(Unit& rUnit, MoveOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder);
    void Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder);

    void ResolveCombatStub_(Unit& rAttacker);
    void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval);
    bool TryEnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter);
    bool TryFungusStep_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
    void ClearFungusCharge_(MoveOrder_t& rMoveOrder);

    StepEvaluator m_steps;
};

} // namespace ac
