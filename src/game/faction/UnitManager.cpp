#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/MovementRules.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/Faction.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ac
{

UnitManager::UnitManager(Faction& rFaction)
    : m_rFaction(rFaction)
{
}

Unit& UnitManager::CreateUnit(UnitId_t unitId, const UnitDesign& rDesign,
                              UnitPositionIndex& rPositions, const Tile& rTile,
                              BaseManager* pHomeBase)
{
    if (!CanPlaceUnitOnTile(rTile, rPositions))
    {
        throw std::runtime_error("UnitManager: tile (" + std::to_string(rTile.GetX())
                                 + ", " + std::to_string(rTile.GetY())
                                 + ") already holds a unit (single-unit-per-tile rule)");
    }

    auto pUnit = std::make_unique<Unit>(unitId, rDesign, rPositions, rTile, pHomeBase, m_rFaction);
    Unit& rUnit = *pUnit;
    m_units.push_back(std::move(pUnit));
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
    return rUnit;
}

void UnitManager::DestroyUnit(Unit& rUnit)
{
    auto it = std::find_if(m_units.begin(), m_units.end(),
        [&rUnit](const std::unique_ptr<Unit>& pUnit)
        {
            return pUnit.get() == &rUnit;
        });

    if (it == m_units.end())
    {
        throw std::runtime_error("Unit not found in UnitManager");
    }

    OnUnitDestroyed.Emit(rUnit);
    m_units.erase(it);
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
}

Unit* UnitManager::GetNextAvailableUnit(const Unit* pAfter) const
{
    auto requiresOrders = [](const Unit& rUnit)
    {
        return !rUnit.GetOrder().has_value() && rUnit.GetMoveFragmentsRemaining() > 0;
    };

    Unit* pFirst = nullptr;
    bool bSeenAfter = (pAfter == nullptr);

    for (const std::unique_ptr<Unit>& pUnit : m_units)
    {
        if (!requiresOrders(*pUnit))
        {
            continue;
        }

        if (!pFirst)
        {
            pFirst = pUnit.get();
        }

        if (bSeenAfter)
        {
            return pUnit.get();
        }

        if (pUnit.get() == pAfter)
        {
            bSeenAfter = true;
        }
    }

    return pFirst;
}

bool UnitManager::HasUnitsRequiringOrders() const
{
    return GetNextAvailableUnit() != nullptr;
}

} // namespace ac
