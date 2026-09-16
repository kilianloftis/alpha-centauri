#include "game/map/UnitPositionIndex.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

const std::vector<Unit*> k_EmptyUnits;

bool EraseUnit_(std::vector<Unit*>& rVec, Unit* pUnit)
{
    const auto newEnd = std::remove(rVec.begin(), rVec.end(), pUnit);
    if (newEnd == rVec.end())
    {
        return false;
    }
    rVec.erase(newEnd, rVec.end());
    return true;
}

} // namespace

const std::vector<Unit*>& UnitPositionIndex::GetUnitsOnTile(const Tile& rTile) const
{
    auto it = m_index.find(&rTile);
    return it != m_index.end() ? it->second.occupants : k_EmptyUnits;
}

const std::vector<Unit*>& UnitPositionIndex::GetCargoOnTile(const Tile& rTile) const
{
    auto it = m_index.find(&rTile);
    return it != m_index.end() ? it->second.cargo : k_EmptyUnits;
}

std::vector<Unit*> UnitPositionIndex::GetAllUnitsOnTile(const Tile& rTile) const
{
    auto it = m_index.find(&rTile);
    if (it == m_index.end())
    {
        return {};
    }
    std::vector<Unit*> all = it->second.occupants;
    all.insert(all.end(), it->second.cargo.begin(), it->second.cargo.end());
    return all;
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
    if (!rUnit.IsEmbarked() && !CanPlaceUnit(rNewTile))
    {
        throw std::logic_error(
            "UnitPositionIndex::MoveUnit: destination already occupied and this world allows "
            "only one unit per tile");
    }
    // Snapshot cargo before mutating occupancy; passengers ride with the carrier.
    const std::vector<Unit*> cargo = rUnit.GetCargo();
    RemoveFromTile_(rUnit);
    TileUnits_t& rDest = m_index[&rNewTile];
    if (rUnit.IsEmbarked())
    {
        rDest.cargo.push_back(&rUnit);
    }
    else
    {
        rDest.occupants.push_back(&rUnit);
    }
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
    for (const auto& [pTile, rEntry] : m_index)
    {
        for (const Unit* pUnit : rEntry.occupants)
        {
            if (pUnit)
            {
                rVisit(*pUnit);
            }
        }
        for (const Unit* pUnit : rEntry.cargo)
        {
            if (pUnit)
            {
                rVisit(*pUnit);
            }
        }
    }
}

bool UnitPositionIndex::CanPlaceUnit(const Tile& rTile) const
{
    if (!m_bSingleUnitPerTile)
    {
        return true;
    }
    return GetUnitsOnTile(rTile).empty();
}

void UnitPositionIndex::Register_(Unit& rUnit, const Tile& rTile)
{
    m_index[&rTile].occupants.push_back(&rUnit);
}

void UnitPositionIndex::Unregister_(Unit& rUnit)
{
    RemoveFromTile_(rUnit);
}

void UnitPositionIndex::NoteEmbarked_(Unit& rUnit)
{
    auto it = m_index.find(rUnit.m_pTile);
    if (it == m_index.end())
    {
        throw std::logic_error("UnitPositionIndex::NoteEmbarked_: unit's tile has no entry");
    }
    if (!EraseUnit_(it->second.occupants, &rUnit))
    {
        throw std::logic_error(
            "UnitPositionIndex::NoteEmbarked_: unit was not an occupant of its tile");
    }
    it->second.cargo.push_back(&rUnit);
}

void UnitPositionIndex::NoteDisembarked_(Unit& rUnit)
{
    auto it = m_index.find(rUnit.m_pTile);
    if (it == m_index.end())
    {
        throw std::logic_error("UnitPositionIndex::NoteDisembarked_: unit's tile has no entry");
    }
    if (!EraseUnit_(it->second.cargo, &rUnit))
    {
        throw std::logic_error(
            "UnitPositionIndex::NoteDisembarked_: unit was not cargo on its tile");
    }
    it->second.occupants.push_back(&rUnit);
}

void UnitPositionIndex::RemoveFromTile_(Unit& rUnit)
{
    // rUnit.m_pTile is maintained exclusively by this class, so the lookup cannot miss.
    // Search both lists: ClearCargoLinks_ may clear m_pCarrier before Unregister_, so
    // IsEmbarked() is not a reliable guide to which vector still holds the pointer.
    auto it = m_index.find(rUnit.m_pTile);
    if (it == m_index.end())
    {
        throw std::logic_error("UnitPositionIndex: unit's tile has no occupancy entry");
    }
    TileUnits_t& rEntry = it->second;
    if (!EraseUnit_(rEntry.occupants, &rUnit) && !EraseUnit_(rEntry.cargo, &rUnit))
    {
        throw std::logic_error("UnitPositionIndex: unit not found on its tile");
    }
    if (rEntry.occupants.empty() && rEntry.cargo.empty())
    {
        m_index.erase(it);
    }
}

} // namespace ac
