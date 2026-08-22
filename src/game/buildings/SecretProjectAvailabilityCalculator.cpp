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

bool SecretProjectAvailabilityCalculator::IsUnavailable(const std::string& rBuildingId) const
{
    // TODO(difficulty): when rules.ai_secret_projects_require_human_prereq, AI may not start
    // an SP until a human faction has the prerequisite tech.
    // A destroyed project is gone for good — unavailable, but owned by nobody.
    return m_rGameState.IsSecretProjectDestroyed(rBuildingId)
           || IsOwnedByAnyFaction(rBuildingId);
}

bool SecretProjectAvailabilityCalculator::IsOwnedByAnyFaction(const std::string& rBuildingId) const
{
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
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
