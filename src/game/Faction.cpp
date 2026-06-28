#include "game/Faction.h"

#include "game/buildings/BuildingRegistry.h"
#include "game/GameDataContext.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/map/WorldMap.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include <iostream>
#include "game/faction/AIProfile.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/Diplomacy.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"

namespace ac
{

Faction::Faction(const BuildingRegistry* pBuildingRegistry, const TechRegistry* pTechRegistry,
                 const SocialPolicyRegistry* pSocialPolicyRegistry,
                 TechCostCalculator* pTechCostCalculator,
                 const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator)
    : m_pBuildingRegistry(pBuildingRegistry)
    , m_pPopTypeAvailabilityCalculator(pPopTypeAvailabilityCalculator)
    , m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pEconomy(std::make_unique<EconomyManager>())
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(pTechRegistry, pTechCostCalculator))
    , m_pDiplomacy(nullptr)
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(pSocialPolicyRegistry))
{
}

Faction::~Faction()
{
}

void Faction::AddEnergy(int amount)
{
    m_energy += amount;
}

int Faction::GetEnergy() const
{
    return m_energy;
}

EconomyManager* Faction::GetEconomyManager() const
{
    return m_pEconomy.get();
}

void Faction::AddBase(std::unique_ptr<BaseManager> pBase)
{
    if (pBase)
    {
        m_bases.push_back(std::move(pBase));
    }
}

BaseManager* Faction::CreateBase(FactionId factionId, int baseId, const std::string& name, const Tile* pTile,
                                  const GameDataContext& rDataContext,
                                  const WorldMap& rWorldMap)
{
    auto pBase = std::make_unique<BaseManager>(
        *pTile,
        rDataContext.buildingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popTypeAvailabilityCalculator.get(),
        rDataContext.popCompositionCalculator.get(),
        rWorldMap,
        m_pResearch.get(),
        m_pEconomy.get(),
        rDataContext.productionCostCalculator.get(),
        rDataContext.growthCalculator.get());
    pBase->SetFactionId(factionId);
    pBase->SetBaseId(baseId);
    pBase->SetName(name);

    pBase->GetWorkerAssignments().UnassignAll();
    pBase->AutoAssignWorkers();

    std::cout << "Created base '" << name << "' with population: " << pBase->GetBaseSize()
              << " (workers: " << pBase->GetPopWorkerCount() << ")\n";

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));
    return pRawBase;
}

BaseManager* Faction::GetBase(size_t index)
{
    if (index < m_bases.size())
    {
        return m_bases[index].get();
    }
    return nullptr;
}

const BaseManager* Faction::GetBase(size_t index) const
{
    if (index < m_bases.size())
    {
        return m_bases[index].get();
    }
    return nullptr;
}

void Faction::CollectBaseResources()
{
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->CollectResources();
        }
    }
}

void Faction::AddResearchPoints(int points)
{
    if (m_pResearch)
    {
        m_pResearch->AddResearchPoints(points);
    }
}

int Faction::GetResearchPoints() const
{
    return m_pResearch ? m_pResearch->GetAccumulatedPoints() : 0;
}

Military& Faction::GetMilitary()
{
    return *m_pMilitary;
}

const Military& Faction::GetMilitary() const
{
    return *m_pMilitary;
}

ResearchManager* Faction::GetResearchManager() const
{
    return m_pResearch.get();
}

std::vector<const BuildingConfig_t*> Faction::GetDiscoveredBuildings() const
{
    if (!m_pBuildingRegistry || !m_pResearch)
    {
        throw std::runtime_error("BuildingRegistry or ResearchManager not initialized");
    }

    const std::vector<std::string>& discoveredTechs = m_pResearch->GetDiscoveredTechs();

    std::vector<const BuildingConfig_t*> discovered;
    for (const BuildingConfig_t& rConfig : m_pBuildingRegistry->GetAll())
    {
        if (rConfig.IsDiscovered(discoveredTechs))
        {
            discovered.push_back(&rConfig);
        }
    }
    return discovered;
}

bool Faction::SetSocialPolicy(SocialCategory category, const std::string& policyId)
{
    return m_pSocialEngineering ? m_pSocialEngineering->SetActivePolicy(category, policyId) : false;
}

const SocialPolicyConfig* Faction::GetSocialPolicy(SocialCategory category) const
{
    return m_pSocialEngineering ? m_pSocialEngineering->GetActivePolicy(category) : nullptr;
}

SocialScores Faction::GetSocialScores() const
{
    return m_pSocialEngineering ? m_pSocialEngineering->GetCombinedScores() : SocialScores{};
}

std::vector<const PopTypeConfig_t*> Faction::GetAvailablePopTypes() const
{
    if (!m_pPopTypeAvailabilityCalculator || !m_pResearch)
    {
        throw std::runtime_error("Faction::GetAvailablePopTypes: Missing calculator or research manager");
    }

    return m_pPopTypeAvailabilityCalculator->GetAvailable(m_pResearch->GetDiscoveredTechs());
}

std::vector<const SocialPolicyConfig*> Faction::GetAvailableSocialPolicies(
    SocialCategory category,
    const std::vector<std::string>& rDiscoveredTechIds) const
{
    return m_pSocialEngineering
        ? m_pSocialEngineering->GetAvailablePolicies(category, rDiscoveredTechIds)
        : std::vector<const SocialPolicyConfig*>{};
}

} // namespace ac
