#pragma once

#include "lib/Signal.h"
#include <unordered_map>
#include <vector>

namespace ac
{

class Unit;
class Tile;

// World-scoped owner of unit-position state. A unit registers itself here for its whole
// life (Unit's constructor/destructor — RAII, so a destroyed unit can never linger in the
// index), and every move goes through MoveUnit, which updates the per-tile occupancy and
// the unit's own tile pointer together — the two can never desync because only this class
// writes them. Placement legality (stacking, etc.) is enforced by callers (MovementRules /
// StepEvaluator), not here. Visibility rebuild lives on Faction; GameState wires
// OnUnitMoved to it (same role GameState plays for OnUnitDestroyed side effects).
class UnitPositionIndex
{
public:
    UnitPositionIndex() = default;
    ~UnitPositionIndex() = default;

    // Non-movable: registered units hold a reference back to this index.
    UnitPositionIndex(const UnitPositionIndex&) = delete;
    UnitPositionIndex& operator=(const UnitPositionIndex&) = delete;

    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;

    // Move rUnit to rNewTile, updating the occupancy and the unit's tile pointer together.
    // Moving a unit onto its own tile is a no-op. Does not enforce stacking — callers must
    // have already established that the destination is legal. Emits OnUnitMoved after a
    // real move so observers (GameState → Faction::RebuildVisibility) can react.
    void MoveUnit(Unit& rUnit, const Tile& rNewTile);

    // Fired from MoveUnit after occupancy and the unit's tile pointer are updated.
    Signal<Unit&> OnUnitMoved;

private:
    // Registration happens exclusively in Unit's constructor/destructor.
    friend class Unit;
    void Register_(Unit& rUnit, const Tile& rTile);
    void Unregister_(Unit& rUnit);

    void RemoveFromTile_(Unit& rUnit);

    std::unordered_map<const Tile*, std::vector<Unit*>> m_index;
};

} // namespace ac
