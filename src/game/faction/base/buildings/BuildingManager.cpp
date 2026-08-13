#include "game/faction/base/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/ResearchManager.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

BuildingManager::BuildingManager(const BuildingRegistry& rBuildingRegistry,
                                 const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
                                 const ResearchManager& rResearchManager)
    : m_rRegistry(rBuildingRegistry)
    , m_pResearch(&rResearchManager)
    , m_pSecretProjectCalculator(pSecretProjectCalculator)
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
    if (rConfig.IsStockpile())
    {
        return false;
    }
    if (!rConfig.allowMultiple && DoesBuildingExist_(buildingId))
    {
        return false;
    }
    if (rConfig.bIsSecretProject)
    {
        if (!m_pSecretProjectCalculator)
        {
            throw std::runtime_error(
                "BuildingManager::CanAddBuilding: '" + buildingId + "' is a secret project, but "
                "this base has no SecretProjectAvailabilityCalculator to check uniqueness");
        }
        return !m_pSecretProjectCalculator->IsUnavailable(buildingId);
    }
    return true;
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

void BuildingManager::DestroyBuilding(const BuildingId_t& buildingId)
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
    if (!m_pSecretProjectCalculator)
    {
        // The one optional dependency, and the only thing that can be missing here: without a
        // session there is no way to know which secret projects are already built, and
        // guessing would offer the player a project that cannot be constructed.
        throw std::runtime_error(
            "BuildingManager::GetBuildingsAvailableForConstruction: this base has no "
            "SecretProjectAvailabilityCalculator, so build availability cannot be resolved");
    }

    const std::vector<std::string>& discoveredTechs = m_pResearch->GetDiscoveredTechs();
    for (const BuildingConfig_t& rBuilding : m_rRegistry.GetAll())
    {
        const bool bSecretProjectCompleted = rBuilding.bIsSecretProject
                && m_pSecretProjectCalculator->IsUnavailable(rBuilding.id);
        // Stockpile items stay in this list so the build menu can queue them, even though
        // CanAddBuilding is false (they are never constructed as facilities).
        if (rBuilding.IsAvailable(discoveredTechs)
                && (rBuilding.allowMultiple || !DoesBuildingExist_(rBuilding.id))
                && !bSecretProjectCompleted)
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
