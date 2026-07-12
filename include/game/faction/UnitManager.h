#pragma once

#include "game/units/Unit.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include <memory>
#include <vector>

namespace ac
{

class UnitDesign;
class Tile;
class BaseManager;
class Faction;
class UnitPositionIndex;

class UnitManager
{
public:
    explicit UnitManager(Faction& rFaction);
    ~UnitManager() = default;

    // The unit registers itself on rTile in rPositions for its lifetime (see Unit's
    // constructor); throws if the stacking rule forbids the tile. unitId must be unique
    // across the game (caller: GameState::AllocateUnitId).
    Unit& CreateUnit(UnitId_t unitId, const UnitDesign& rDesign, UnitPositionIndex& rPositions,
                     const Tile& rTile, BaseManager* pHomeBase = nullptr);
    void DestroyUnit(Unit& rUnit);

    // Iterate units by reference without exposing the owning unique_ptrs.
    auto Units() { return DerefView(m_units); }
    auto Units() const { return DerefView(m_units); }
    Unit* GetNextAvailableUnit() const;

    // Bumped on every unit creation/destruction; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

    // Fired from DestroyUnit before the unit is erased, so observers (e.g. WorldView's unit
    // selection) can drop any reference to it while it is still valid.
    Signal<Unit&> OnUnitDestroyed;

private:
    Faction& m_rFaction;
    std::vector<std::unique_ptr<Unit>> m_units;
    Revision m_revision;
};

} // namespace ac
