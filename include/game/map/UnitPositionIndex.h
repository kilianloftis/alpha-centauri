#pragma once

#include "lib/Signal.h"
#include <functional>
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
// writes them. It also owns the stacking rule and enforces it in MoveUnit — see
// SetSingleUnitPerTile and CanPlaceUnit, the single definition callers delegate to.
//
// Caveat: MoveUnit is not the only way occupancy changes. Register_ (a unit being created) and
// Unit::Disembark (an embarked unit becoming an independent occupant) both bypass the check, so
// an existing violation can persist even though no new move may create one. Closing that needs
// a decision about what unloading onto a full tile should *do*, which is a game rule, not a
// refactor — see docs/full-review-fix-prompts/07-units-movement-and-orders.md.
//
// Visibility rebuild lives on Faction; GameState wires OnUnitMoved to it (same role GameState
// plays for OnUnitDestroyed side effects).
class UnitPositionIndex
{
public:
    UnitPositionIndex() = default;
    ~UnitPositionIndex() = default;

    // Non-movable: registered units hold a reference back to this index.
    UnitPositionIndex(const UnitPositionIndex&) = delete;
    UnitPositionIndex& operator=(const UnitPositionIndex&) = delete;

    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;

    // Visit every registered unit once, in unspecified order. O(units) — the index only holds
    // occupied tiles, so this is the way to sweep all units without walking the whole map.
    void ForEachUnit(const std::function<void(const Unit&)>& rVisit) const;

    // Stacking rule: when set, at most one non-embarked unit may occupy a tile. It lives here,
    // beside the occupancy it constrains and scoped to one world, rather than in a file-scope
    // global in MovementRules — a process-wide switch meant two sessions could not disagree and
    // a test could leak the setting into the next case.
    void SetSingleUnitPerTile(bool bSingleUnitPerTile) { m_bSingleUnitPerTile = bSingleUnitPerTile; }
    bool IsSingleUnitPerTile() const { return m_bSingleUnitPerTile; }

    // The one definition of "may a unit occupy rTile". Embarked units do not count as
    // occupants — they share their carrier's tile by construction. MovementRules and
    // StepEvaluator delegate here rather than re-deriving it: three readings of one rule is how
    // they drift, and two of them already disagreed about embarked units.
    bool CanPlaceUnit(const Tile& rTile) const;

    // Move rUnit to rNewTile, updating the occupancy and the unit's tile pointer together.
    // Moving a unit onto its own tile is a no-op. Throws std::logic_error if the destination
    // violates the stacking rule — callers that plan legality first (StepEvaluator) still
    // should, but one that forgets cannot overstack the index. Embarked passengers are exempt
    // and are towed with their carrier. Emits OnUnitMoved after a real move so observers
    // (GameState → Faction::RebuildVisibility) can react.
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
    bool m_bSingleUnitPerTile = false;
};

} // namespace ac
