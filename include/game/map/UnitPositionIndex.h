#pragma once

#include <unordered_map>
#include <vector>

namespace ac
{

class Unit;
class Tile;

class UnitPositionIndex
{
public:
    UnitPositionIndex() = default;
    ~UnitPositionIndex() = default;

    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;

    void OnUnitPlaced(Unit& rUnit, Tile& rTile);
    void OnUnitRemoved(Unit& rUnit);
    void OnUnitMoved(Unit& rUnit, Tile& rNewTile);

private:
    std::unordered_map<const Tile*, std::vector<Unit*>> m_index;
};

} // namespace ac
