#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"

namespace ac
{

SecretProjectAvailabilityCalculator::SecretProjectAvailabilityCalculator(const GameState& rGameState)
    : m_rGameState(rGameState)
{
}

bool SecretProjectAvailabilityCalculator::IsCompleted(const std::string& rBuildingId) const
{
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            for (const BuildingConfig_t* pBuilding : rBase.GetBuildings())
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
