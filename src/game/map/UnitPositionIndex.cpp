#include "game/map/UnitPositionIndex.h"
#include "game/units/Unit.h"
#include <algorithm>

namespace ac
{

const std::vector<Unit*>& UnitPositionIndex::GetUnitsOnTile(const Tile& rTile) const
{
    static const std::vector<Unit*> empty;
    auto it = m_index.find(&rTile);
    return it != m_index.end() ? it->second : empty;
}

void UnitPositionIndex::OnUnitPlaced(Unit& rUnit, Tile& rTile)
{
    m_index[&rTile].push_back(&rUnit);
}

void UnitPositionIndex::OnUnitRemoved(Unit& rUnit)
{
    auto it = m_index.find(&rUnit.GetTile());
    if (it == m_index.end())
        return;

    auto& rVec = it->second;
    rVec.erase(std::remove(rVec.begin(), rVec.end(), &rUnit), rVec.end());
}

void UnitPositionIndex::OnUnitMoved(Unit& rUnit, Tile& rNewTile)
{
    OnUnitRemoved(rUnit);
    OnUnitPlaced(rUnit, rNewTile);
}

} // namespace ac
