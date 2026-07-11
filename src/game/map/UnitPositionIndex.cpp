#include "game/map/UnitPositionIndex.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/Faction.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ac
{

bool UnitPositionIndex::s_bSingleUnitPerTile = false;

void UnitPositionIndex::SetSingleUnitPerTile(bool bSingleUnitPerTile)
{
    s_bSingleUnitPerTile = bSingleUnitPerTile;
}

bool UnitPositionIndex::IsSingleUnitPerTile()
{
    return s_bSingleUnitPerTile;
}

const std::vector<Unit*>& UnitPositionIndex::GetUnitsOnTile(const Tile& rTile) const
{
    static const std::vector<Unit*> empty;
    auto it = m_index.find(&rTile);
    return it != m_index.end() ? it->second : empty;
}

bool UnitPositionIndex::TryMoveUnit(Unit& rUnit, const Tile& rNewTile)
{
    if (rUnit.m_pTile == &rNewTile)
    {
        return true;
    }
    if (!CanPlace_(rNewTile))
    {
        return false;
    }
    RemoveFromTile_(rUnit);
    m_index[&rNewTile].push_back(&rUnit);
    rUnit.m_pTile = &rNewTile;
    rUnit.GetFaction().RebuildVisibility();
    return true;
}

void UnitPositionIndex::Register_(Unit& rUnit, const Tile& rTile)
{
    if (!CanPlace_(rTile))
    {
        throw std::runtime_error("UnitPositionIndex: tile (" + std::to_string(rTile.GetX())
                                 + ", " + std::to_string(rTile.GetY())
                                 + ") already holds a unit (single-unit-per-tile rule)");
    }
    m_index[&rTile].push_back(&rUnit);
}

void UnitPositionIndex::Unregister_(Unit& rUnit)
{
    RemoveFromTile_(rUnit);
}

bool UnitPositionIndex::CanPlace_(const Tile& rTile) const
{
    return !s_bSingleUnitPerTile || GetUnitsOnTile(rTile).empty();
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
