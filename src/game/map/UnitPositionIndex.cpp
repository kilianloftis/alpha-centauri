#include "game/map/UnitPositionIndex.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/Faction.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

const std::vector<Unit*>& UnitPositionIndex::GetUnitsOnTile(const Tile& rTile) const
{
    static const std::vector<Unit*> empty;
    auto it = m_index.find(&rTile);
    return it != m_index.end() ? it->second : empty;
}

void UnitPositionIndex::MoveUnit(Unit& rUnit, const Tile& rNewTile)
{
    if (rUnit.m_pTile == &rNewTile)
    {
        return;
    }
    RemoveFromTile_(rUnit);
    m_index[&rNewTile].push_back(&rUnit);
    rUnit.m_pTile = &rNewTile;
    rUnit.GetFaction().RebuildVisibility();
}

void UnitPositionIndex::Register_(Unit& rUnit, const Tile& rTile)
{
    m_index[&rTile].push_back(&rUnit);
}

void UnitPositionIndex::Unregister_(Unit& rUnit)
{
    RemoveFromTile_(rUnit);
}

void UnitPositionIndex::RemoveFromTile_(Unit& rUnit)
{
    // rUnit.m_pTile is maintained exclusively by this class, so the lookup cannot miss.
    auto it = m_index.find(rUnit.m_pTile);
    if (it == m_index.end())
    {
        throw std::logic_error("UnitPositionIndex: unit's tile has no occupancy entry");
    }
    auto& rVec = it->second;
    rVec.erase(std::remove(rVec.begin(), rVec.end(), &rUnit), rVec.end());
    if (rVec.empty())
    {
        m_index.erase(it);
    }
}

} // namespace ac
