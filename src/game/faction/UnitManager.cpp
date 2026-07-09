#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/map/Tile.h"
#include "game/Faction.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

UnitManager::UnitManager(Faction& rFaction)
    : m_rFaction(rFaction)
{
}

Unit& UnitManager::CreateUnit(const UnitDesign& rDesign, const Tile& rTile, BaseManager* pHomeBase)
{
    auto pUnit = std::make_unique<Unit>(rDesign, rTile, pHomeBase, m_rFaction);
    Unit& rUnit = *pUnit;
    m_units.push_back(std::move(pUnit));
    m_revision.Bump();
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

    m_units.erase(it);
    m_revision.Bump();
}

Unit* UnitManager::GetNextAvailableUnit() const
{
    for (const std::unique_ptr<Unit>& pUnit : m_units)
    {
        if (!pUnit->GetOrder().has_value() && pUnit->GetMovesRemaining() > 0)
            return pUnit.get();
    }
    return nullptr;
}

} // namespace ac
