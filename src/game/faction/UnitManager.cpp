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

Unit& UnitManager::CreateUnit(const UnitDesign& rDesign, Tile& rTile, BaseManager* pHomeBase)
{
    auto pUnit = std::make_unique<Unit>(rDesign, rTile, pHomeBase, m_rFaction);
    Unit& rUnit = *pUnit;
    m_units.push_back(std::move(pUnit));
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
}

const std::vector<std::unique_ptr<Unit>>& UnitManager::GetUnits() const
{
    return m_units;
}

} // namespace ac
