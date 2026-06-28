#pragma once

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

    Unit& CreateUnit(const UnitDesign& rDesign, Tile& rTile, BaseManager* pHomeBase = nullptr);
    void DestroyUnit(Unit& rUnit);

    const std::vector<std::unique_ptr<Unit>>& GetUnits() const;

private:
    Faction& m_rFaction;
    std::vector<std::unique_ptr<Unit>> m_units;
};

} // namespace ac
