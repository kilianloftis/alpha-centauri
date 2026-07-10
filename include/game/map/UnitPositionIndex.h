#pragma once

#include <unordered_map>
#include <vector>

namespace ac
{

class Unit;
class Tile;

// World-scoped owner of unit-position state. A unit registers itself here for its whole
// life (Unit's constructor/destructor — RAII, so a destroyed unit can never linger in the
// index), and every move goes through TryMoveUnit, which updates the per-tile occupancy
// and the unit's own tile pointer together — the two can never desync because only this
// class writes them.
//
// Also enforces the stacking rule: the original game allows any number of units per tile
// (the default); SetSingleUnitPerTile(true) restricts every tile to at most one unit.
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
    // Returns false (changing nothing) if the stacking rule forbids the destination.
    // Moving a unit onto its own tile is a successful no-op.
    bool TryMoveUnit(Unit& rUnit, const Tile& rNewTile);

    // Stacking rule: when true, at most one unit may occupy a tile; when false (default),
    // units stack without limit, as in the original game.
    // TODO: static stand-in until a real game-configuration system exists.
    static void SetSingleUnitPerTile(bool bSingleUnitPerTile);
    static bool IsSingleUnitPerTile();

private:
    // Registration happens exclusively in Unit's constructor/destructor.
    friend class Unit;
    void Register_(Unit& rUnit, const Tile& rTile);
    void Unregister_(Unit& rUnit);

    bool CanPlace_(const Tile& rTile) const;
    void RemoveFromTile_(Unit& rUnit);

    static bool s_bSingleUnitPerTile;

    std::unordered_map<const Tile*, std::vector<Unit*>> m_index;
};

} // namespace ac
