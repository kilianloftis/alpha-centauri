#pragma once

#include "lib/Signal.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace ac
{

class Unit;
class Tile;

// Owns unit positions: units register for life, and every move goes through MoveUnit.
// Tiles may hold any number of units (SMAC stacking).
class UnitPositionIndex
{
public:
    UnitPositionIndex() = default;
    ~UnitPositionIndex() = default;

    // Non-movable: registered units hold a reference back to this index.
    UnitPositionIndex(const UnitPositionIndex&) = delete;
    UnitPositionIndex& operator=(const UnitPositionIndex&) = delete;

    // Occupants on rTile (non-embarked). Cargo is excluded — carried units are aboard a
    // carrier, not present on the tile for occupancy, ZOC, or load-site projection.
    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;
    // Embarked units whose carrier stands on rTile.
    const std::vector<Unit*>& GetCargoOnTile(const Tile& rTile) const;
    // Occupants and cargo together (fresh vector). Prefer GetUnitsOnTile / GetCargoOnTile;
    // use this for garrison/defence and UI that must see every unit sharing the tile.
    std::vector<Unit*> GetAllUnitsOnTile(const Tile& rTile) const;

    // Visit every registered unit once, in unspecified order. O(units) — the index only holds
    // occupied tiles, so this is the way to sweep all units without walking the whole map.
    void ForEachUnit(const std::function<void(const Unit&)>& rVisit) const;

    // Moves rUnit, keeping occupancy and its tile pointer together. Embarked passengers are
    // towed. Emits OnUnitMoved after a real move.
    void MoveUnit(Unit& rUnit, const Tile& rNewTile);

    // Fired from MoveUnit after occupancy and the unit's tile pointer are updated.
    Signal<Unit&> OnUnitMoved;

private:
    // Registration happens exclusively in Unit's constructor/destructor.
    friend class Unit;
    friend class UnitTransport;
    void Register_(Unit& rUnit, const Tile& rTile);
    void Unregister_(Unit& rUnit);

    // EmbarkInto / Disembark (and orphaning in ClearCargoLinks_) move the unit between the
    // per-tile occupant and cargo lists without changing its tile pointer.
    void NoteEmbarked_(Unit& rUnit);
    void NoteDisembarked_(Unit& rUnit);

    void RemoveFromTile_(Unit& rUnit);

    struct TileUnits_t
    {
        std::vector<Unit*> occupants;
        std::vector<Unit*> cargo;
    };

    std::unordered_map<const Tile*, TileUnits_t> m_index;
};

} // namespace ac
