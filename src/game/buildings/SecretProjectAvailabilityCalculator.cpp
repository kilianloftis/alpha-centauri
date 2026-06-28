#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"

namespace ac
{

SecretProjectAvailabilityCalculator::SecretProjectAvailabilityCalculator(
    const std::vector<std::unique_ptr<Faction>>& rFactions)
    : m_pFactions(&rFactions)
{
}

bool SecretProjectAvailabilityCalculator::IsCompleted(const std::string& rBuildingId) const
{
    for (const auto& pFaction : *m_pFactions)
    {
        for (const auto& pBase : pFaction->GetBases())
        {
            for (const BuildingConfig_t* pBuilding : pBase->GetBuildings())
            {
                if (pBuilding->id == rBuildingId)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace ac
