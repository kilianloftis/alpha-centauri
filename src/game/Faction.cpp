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
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/effects/ActiveEffect.h"

namespace ac
{

Faction::Faction(const BuildingRegistry* pBuildingRegistry, const TechRegistry* pTechRegistry,
                 const SocialPolicyRegistry* pSocialPolicyRegistry,
                 const SocialRatingRegistry* pSocialRatingRegistry,
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
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(pSocialPolicyRegistry,
                                                                        pSocialRatingRegistry))
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

BaseManager* Faction::CreateBase(FactionId factionId, int baseId, const std::string& name, Tile* pTile,
                                  const GameDataContext& rDataContext,
                                  TileEffectsContext& rTileEffects)
{
    auto pBase = std::make_unique<BaseManager>(
        *pTile,
        rDataContext.buildingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popTypeAvailabilityCalculator.get(),
        rDataContext.popCompositionCalculator.get(),
        rTileEffects,
        m_pResearch.get(),
        m_pEconomy.get(),
        rDataContext.productionCostCalculator.get(),
        *rDataContext.growthConfig,
        rDataContext.secretProjectAvailabilityCalculator.get());
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

void Faction::ProduceBaseResources()
{
    const std::vector<ActiveEffect_t> activeEffects = CollectActiveEffects(*this);

    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ProduceResources(activeEffects);
        }
    }
}

void Faction::ApplyBaseGrowth()
{
    const std::vector<ActiveEffect_t> activeEffects = CollectActiveEffects(*this);

    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ApplyGrowth(activeEffects);
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
    if (!m_pSocialEngineering)
    {
        throw std::runtime_error("SocialEngineeringManager not initialized");
    }
    return m_pSocialEngineering->SetActivePolicy(category, policyId);
}

const SocialPolicyConfig* Faction::GetSocialPolicy(SocialCategory category) const
{
    if (!m_pSocialEngineering)
    {
        throw std::runtime_error("SocialEngineeringManager not initialized");
    }
    return m_pSocialEngineering->GetActivePolicy(category);
}

std::vector<ActiveEffect_t> Faction::CollectBuildingEffects() const
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pBase : m_bases)
    {
        if (!pBase) continue;
        const auto baseEffects = pBase->CollectBuildingEffects();
        result.insert(result.end(), baseEffects.begin(), baseEffects.end());
    }

    if (!m_pBuildingRegistry)
    {
        return result;
    }

    std::vector<const BaseManager*> bases;
    for (const auto& pBase : m_bases)
    {
        bases.push_back(pBase.get());
    }

    return ExpandGrantBuildingEffects(std::move(result), *m_pBuildingRegistry, bases);
}

std::vector<ActiveEffect_t> Faction::CollectSocialEffects() const
{
    return m_pSocialEngineering ? m_pSocialEngineering->CollectEffects() : std::vector<ActiveEffect_t>{};
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
    if (!m_pSocialEngineering)
    {
        return std::vector<const SocialPolicyConfig*>{};
    }
    return m_pSocialEngineering->GetAvailablePolicies(category, rDiscoveredTechIds);
}

} // namespace ac
