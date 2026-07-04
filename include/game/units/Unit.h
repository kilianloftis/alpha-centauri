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

class Unit
{
public:
    Unit(const UnitDesign& rDesign,
         const Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction);
    ~Unit() = default;

    const UnitDesign& GetDesign() const;

    int GetBaseCost() const;
    int GetAttack() const;
    // Attack against a specific defender, applying any conditional modifiers that match the
    // defender's tile (e.g. a bonus vs bases or vs a terrain type). GetAttack() is the
    // context-free base value.
    int GetAttackAgainst(const Unit& rDefender) const;
    int GetDefense() const;
    int GetMovement() const;
    int GetHitPoints() const;
    int GetDisengageChance() const;
    int GetFuel() const;
    int GetDamageFromOutOfFuel() const;
    bool IsFlight() const;
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
    void SetTile(const Tile& rTile);
    void SetHomeBase(BaseManager* pHomeBase);

    std::optional<UnitOrder_t>& GetOrder();
    const std::optional<UnitOrder_t>& GetOrder() const;
    void SetOrder(const UnitOrder_t& rOrder);
    void ClearOrder();

private:
    const UnitDesign& m_rDesign;
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
