#pragma once

#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include <optional>
#include <string>

namespace ac
{

class Tile;
class BaseManager;
class Faction;
class UnitPositionIndex;

using UnitId_t = int;

class Unit
{
public:
    // Registers the unit on rTile in rPositions for its whole life (unregistered in the
    // destructor). The index is the single owner of unit-position state: it keeps
    // GetTile() in sync on every move (UnitPositionIndex::MoveUnit) and there is no
    // other way to change a unit's position. Placement legality is the caller's job
    // (UnitManager::CreateUnit). unitId must be unique for the life of the game
    // (WorldMap's unit IdAllocator).
    Unit(UnitId_t unitId,
         const UnitDesign& rDesign,
         UnitPositionIndex& rPositions,
         const Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction);
    ~Unit();

    UnitId_t GetUnitId() const;

    const UnitDesign& GetDesign() const;

    // Live-unit stat / flag resolution (design effects + FactionUnits). Prefer the free
    // ResolveStat / ResolveFlag overloads; these forward to them.
    int GetStat(StatId_t statId) const;
    int GetStat(StatId_t statId, const EffectContext_t& rCtx) const;
    bool GetFlag(RuleFlagId_t flagId) const;
    UnitDomain_t GetDomain() const;

    const Tile& GetTile() const;
    BaseManager* GetHomeBase() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;

    int GetCurrentHp() const;
    int GetCurrentFuel() const;
    // Chassis Movement stat in move-points (not fragments).
    int GetMovementPoints() const;
    // Remaining movement in fragments (k_moveFragmentsPerPoint per Movement point).
    int GetMoveFragmentsRemaining() const;
    int GetXp() const;

    void SetCurrentHp(int hp);
    void SetCurrentFuel(int fuel);
    // Clamps to [0, MovementPoints * k_moveFragmentsPerPoint].
    void SetMoveFragmentsRemaining(int fragments);
    void SetXp(int xp);
    void SetHomeBase(BaseManager* pHomeBase);

    std::optional<UnitOrder_t>& GetOrder();
    const std::optional<UnitOrder_t>& GetOrder() const;
    void SetOrder(const UnitOrder_t& rOrder);
    void ClearOrder();

private:
    // The index maintains m_pTile alongside its occupancy lists (MoveUnit).
    friend class UnitPositionIndex;

    UnitId_t m_unitId;
    const UnitDesign& m_rDesign;
    UnitPositionIndex& m_rPositions;
    const Tile* m_pTile;
    BaseManager* m_pHomeBase;
    Faction& m_rFaction;

    int m_currentHp;
    int m_currentFuel;
    int m_moveFragmentsRemaining;
    int m_xp;
    std::optional<UnitOrder_t> m_order;
};

} // namespace ac
