#include "game/map/UnitPositionIndex.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
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
    // Enforced at the mutation boundary, not only at the planning layer. Callers that reason
    // about legality first (StepEvaluator, UnitManager::CreateUnit) still do, but a caller that
    // forgets can no longer overstack the index behind their back. Embarked passengers are
    // exempt: they ride with their carrier and are not independent occupants.
    if (m_bSingleUnitPerTile && !rUnit.IsEmbarked() && !CanPlaceUnit_(rNewTile))
    {
        throw std::logic_error(
            "UnitPositionIndex::MoveUnit: destination already occupied and this world allows "
            "only one unit per tile");
    }
    // Snapshot cargo before mutating occupancy; passengers ride with the carrier.
    const std::vector<Unit*> cargo = rUnit.GetCargo();
    RemoveFromTile_(rUnit);
    m_index[&rNewTile].push_back(&rUnit);
    rUnit.m_pTile = &rNewTile;
    OnUnitMoved.Emit(rUnit);
    for (Unit* pPassenger : cargo)
    {
        if (pPassenger && pPassenger->GetCarrier() == &rUnit)
        {
            MoveUnit(*pPassenger, rNewTile);
        }
    }
}

void UnitPositionIndex::ForEachUnit(const std::function<void(const Unit&)>& rVisit) const
{
    for (const auto& [pTile, rUnits] : m_index)
    {
        for (const Unit* pUnit : rUnits)
        {
            if (pUnit)
            {
                rVisit(*pUnit);
            }
        }
    }
}

bool UnitPositionIndex::CanPlaceUnit_(const Tile& rTile) const
{
    // Only non-embarked units count as occupants — a loaded transport's cargo shares its tile.
    const auto it = m_index.find(&rTile);
    if (it == m_index.end())
    {
        return true;
    }
    return std::none_of(it->second.begin(), it->second.end(),
                        [](const Unit* pUnit) { return pUnit && !pUnit->IsEmbarked(); });
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
