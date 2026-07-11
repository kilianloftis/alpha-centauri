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

class Unit
{
public:
    // Registers the unit on rTile in rPositions for its whole life (unregistered in the
    // destructor). The index is the single owner of unit-position state: it keeps
    // GetTile() in sync on every move (UnitPositionIndex::TryMoveUnit) and there is no
    // other way to change a unit's position. Throws if the stacking rule forbids rTile.
    Unit(const UnitDesign& rDesign,
         UnitPositionIndex& rPositions,
         const Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction);
    ~Unit();

    const UnitDesign& GetDesign() const;

    int GetBaseCost() const;
    int GetAttack() const;
    // Attack against a specific defender, applying any conditional modifiers that match the
    // defender's tile (e.g. a bonus vs bases or vs a terrain type). GetAttack() is the
    // context-free base value.
    int GetAttackAgainst(const Unit& rDefender) const;
    int GetDefense() const;
    int GetMovement() const;
    int GetVision() const;
    int GetHitPoints() const;
    int GetDisengageChance() const;
    int GetFuel() const;
    int GetDamageFromOutOfFuel() const;
    bool IsFlight() const;
    bool IsSea() const;
    // Neither flight nor sea.
    bool IsLandUnit() const;
    // True if the unit is not subject to hostile zone of control (flight, or the
    // IgnoreZoneOfControl rule flag used by e.g. probe teams).
    bool IgnoresZoneOfControl() const;
    int GetCargoCapacity() const;
    int GetDifficultTerrainCost() const;
    bool IsSingleUse() const;
    const Tile& GetTile() const;
    BaseManager* GetHomeBase() const;
    Faction& GetFaction() const;

    int GetCurrentHp() const;
    int GetCurrentFuel() const;
    int GetMovesRemaining() const;
    int GetXp() const;

    void SetCurrentHp(int hp);
    void SetCurrentFuel(int fuel);
    void SetMovesRemaining(int moves);
    void SetXp(int xp);
    void SetHomeBase(BaseManager* pHomeBase);

    std::optional<UnitOrder_t>& GetOrder();
    const std::optional<UnitOrder_t>& GetOrder() const;
    void SetOrder(const UnitOrder_t& rOrder);
    void ClearOrder();

private:
    // Live-unit stat resolution: the design's own ThisUnit effects plus any FactionUnits
    // effects active in the owning faction's pool (buildings, policies, pops, ...).
    // UnitDesign's getters stay context-free (intrinsic values, e.g. for the designer UI).
    int ResolveStat_(StatId_t statId) const;
    bool ResolveFlag_(RuleFlagId_t flagId) const;

    // The index maintains m_pTile alongside its occupancy lists (TryMoveUnit).
    friend class UnitPositionIndex;

    const UnitDesign& m_rDesign;
    UnitPositionIndex& m_rPositions;
    const Tile* m_pTile;
    BaseManager* m_pHomeBase;
    Faction& m_rFaction;

    int m_currentHp;
    int m_currentFuel;
    int m_movesRemaining;
    int m_xp;
    std::optional<UnitOrder_t> m_order;
};

} // namespace ac
