#include "game/faction/base/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/ResearchManager.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

BuildingManager::BuildingManager(const BuildingRegistry* pBuildingRegistry,
                                 const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
                                 const ResearchManager* pResearchManager)
    : m_pRegistry(pBuildingRegistry)
    , m_pResearch(pResearchManager)
    , m_pSecretProjectCalculator(pSecretProjectCalculator)
{
}

BuildingManager::~BuildingManager()
{
}

void BuildingManager::AddBuilding(const std::string& buildingId)
{
    const BuildingConfig_t* pConfig = m_pRegistry->Find(buildingId);
    if (!pConfig)
    {
        throw std::runtime_error("Unknown building id '" + buildingId + "'");
    }
    m_buildings.push_back(pConfig);
    m_revision.Bump();
}

void BuildingManager::DestroyBuilding(const std::string& buildingId)
{
    auto it = std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const BuildingConfig_t* pBuilding)
        {
            return buildingId == pBuilding->id;
        });

    if (it != m_buildings.end())
    {
        m_buildings.erase(it);
        m_revision.Bump();
    }
}

const std::vector<const BuildingConfig_t*>& BuildingManager::GetBuildings() const
{
    return m_buildings;
}

std::vector<ActiveEffect_t> BuildingManager::CollectEffects() const
{
    std::vector<ActiveEffect_t> result;
    for (const BuildingConfig_t* pBuilding : m_buildings)
    {
        if (!pBuilding)
        {
            continue;
        }
        AppendActiveEffects(pBuilding->effects, nullptr, pBuilding->id, result);
    }
    return result;
}

std::vector<const BuildingConfig_t*> BuildingManager::GetBuildingsAvailableForConstruction() const
{
    std::vector<const BuildingConfig_t*> available;
    if (!m_pResearch || !m_pRegistry || !m_pSecretProjectCalculator)
    {
        throw std::runtime_error("BuildingManager::GetBuildingsAvailableForConstruction: missing dependencies");
    }

    const std::vector<std::string>& discoveredTechs = m_pResearch->GetDiscoveredTechs();
    for (const BuildingConfig_t& rBuilding : m_pRegistry->GetAll())
    {
        const bool bSecretProjectCompleted = rBuilding.bIsSecretProject
                && m_pSecretProjectCalculator->IsCompleted(rBuilding.id);
        if (rBuilding.IsAvailable(discoveredTechs)
                && (rBuilding.allowMultiple || !DoesBuildingExist_(rBuilding.id))
                && !bSecretProjectCompleted)
        {
            available.push_back(&rBuilding);
        }
    }
    return available;
}

bool BuildingManager::DoesBuildingExist_(const std::string& buildingId) const
{
    return std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const BuildingConfig_t* pBuilding)
        {
            return buildingId == pBuilding->id;
        }) != m_buildings.end();
}

} // namespace ac
