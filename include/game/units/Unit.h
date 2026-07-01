#pragma once

#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace ac
{

class Tile;
class BaseManager;
class Faction;

class Unit
{
public:
    Unit(const UnitDesign& rDesign,
         Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction);
    ~Unit() = default;

    const UnitDesign& GetDesign() const;

    int GetBaseCost() const;
    int GetAttack() const;
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
    std::unordered_map<std::string, double> GetTerrainAttackBonus() const;
    Tile& GetTile() const;
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
    void SetTile(Tile& rTile);
    void SetHomeBase(BaseManager* pHomeBase);

    std::optional<UnitOrder_t>& GetOrder();
    const std::optional<UnitOrder_t>& GetOrder() const;
    void SetOrder(const UnitOrder_t& rOrder);
    void ClearOrder();

private:
    const UnitDesign& m_rDesign;
    Tile* m_pTile;
    BaseManager* m_pHomeBase;
    Faction& m_rFaction;

    int m_currentHp;
    int m_currentFuel;
    int m_movesRemaining;
    int m_xp;
    std::optional<UnitOrder_t> m_order;
};

} // namespace ac
