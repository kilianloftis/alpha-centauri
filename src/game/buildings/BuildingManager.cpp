#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/BuildingRules.h"
#include "game/research/ResearchManager.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

BuildingManager::BuildingManager(const BuildingRegistry& rBuildingRegistry,
                                 const ResearchManager& rResearchManager)
    : m_rRegistry(rBuildingRegistry)
    , m_pResearch(&rResearchManager)
{
}

BuildingManager::~BuildingManager()
{
}

void BuildingManager::RebindResearch(const ResearchManager& rResearch)
{
    m_pResearch = &rResearch;
}

bool BuildingManager::CanAddBuilding(const BuildingId_t& buildingId) const
{
    const BuildingConfig_t& rConfig = m_rRegistry.Get(buildingId);
    return MayHoldBuilding(*this, rConfig);
}

void BuildingManager::AddBuilding(const BuildingId_t& buildingId)
{
    if (!CanAddBuilding(buildingId))
    {
        throw std::runtime_error(
            "BuildingManager::AddBuilding: '" + buildingId + "' cannot be added to this base "
            "(already present and not allowMultiple, or a secret project that is built or "
            "destroyed). Callers that can lose a race must ask CanAddBuilding first.");
    }
    m_buildings.push_back(&m_rRegistry.Get(buildingId));
    m_revision.Bump();
}

bool BuildingManager::HasBuilding(const BuildingId_t& buildingId) const
{
    return DoesBuildingExist_(buildingId);
}

const BuildingConfig_t* BuildingManager::FindBuilding(const BuildingId_t& buildingId) const
{
    for (const BuildingConfig_t* pBuilding : m_buildings)
    {
        if (pBuilding && pBuilding->id == buildingId)
        {
            return pBuilding;
        }
    }
    return nullptr;
}

void BuildingManager::DestroyBuilding(const BuildingId_t& buildingId)
{
    auto it = std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const BuildingConfig_t* pBuilding)
        {
            return buildingId == pBuilding->id;
        });

    if (it != m_buildings.end())
    {
        const BuildingConfig_t& rDestroyed = **it;
        m_buildings.erase(it);
        m_revision.Bump();
        OnBuildingDestroyed.Emit(rDestroyed);
    }
}

const std::vector<const BuildingConfig_t*>& BuildingManager::GetBuildings() const
{
    return m_buildings;
}

std::vector<ActiveEffect_t> BuildingManager::CollectEffects(const BaseManager& rOriginBase) const
{
    std::vector<ActiveEffect_t> result;
    for (const BuildingConfig_t* pBuilding : m_buildings)
    {
        if (!pBuilding)
        {
            continue;
        }
        AppendActiveEffects(pBuilding->effects, &rOriginBase, pBuilding->id, result);
    }
    return result;
}

std::vector<const BuildingConfig_t*> BuildingManager::GetBuildingsAvailableForConstruction() const
{
    std::vector<const BuildingConfig_t*> available;
    const std::vector<std::string>& discoveredTechs = m_pResearch->GetDiscoveredTechs();
    for (const BuildingConfig_t& rBuilding : m_rRegistry.GetAll())
    {
        if (rBuilding.IsAvailable(discoveredTechs) && MayHoldBuilding(*this, rBuilding))
        {
            available.push_back(&rBuilding);
        }
    }
    return available;
}

bool BuildingManager::DoesBuildingExist_(const BuildingId_t& buildingId) const
{
    return std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const BuildingConfig_t* pBuilding)
        {
            return buildingId == pBuilding->id;
        }) != m_buildings.end();
}

} // namespace ac
