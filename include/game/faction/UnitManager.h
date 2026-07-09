#pragma once

#include "lib/DerefView.h"
#include "lib/Revision.h"
#include <memory>
#include <vector>

namespace ac
{

class Unit;
class UnitDesign;
class Tile;
class BaseManager;
class Faction;

class UnitManager
{
public:
    explicit UnitManager(Faction& rFaction);
    ~UnitManager() = default;

    Unit& CreateUnit(const UnitDesign& rDesign, const Tile& rTile, BaseManager* pHomeBase = nullptr);
    void DestroyUnit(Unit& rUnit);

    // Iterate units by reference without exposing the owning unique_ptrs.
    auto Units() { return DerefView(m_units); }
    auto Units() const { return DerefView(m_units); }
    Unit* GetNextAvailableUnit() const;

    // Bumped on every unit creation/destruction; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    Faction& m_rFaction;
    std::vector<std::unique_ptr<Unit>> m_units;
    Revision m_revision;
};

} // namespace ac
