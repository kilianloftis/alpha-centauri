#include "game/Faction.h"

#include "game/GameDataContext.h"
#include "game/map/WorldMap.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include <iostream>
#include "game/faction/AIProfile.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/Diplomacy.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopTypeConfigParser.h"

namespace ac
{

Faction::Faction(const TechRegistry* pTechRegistry, const SocialPolicyRegistry* pSocialPolicyRegistry,
                 TechCostCalculator* pTechCostCalculator, const PopTypeRegistry* pPopTypeRegistry)
    : m_pPopTypeRegistry(pPopTypeRegistry)
    , m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pEconomy(std::make_unique<BaseEconomyManager>())
    , m_pMilitary(nullptr)
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

void Faction::ProcessTurn()
{
    // TODO: Delegate to subsystems
    // m_pEconomy->CalculateIncome();
    // m_pMilitary->UpdateUnits();
    // m_pResearch->AdvanceResearch();
    // m_pDiplomacy->UpdateRelations();
}

void Faction::AddBase(std::unique_ptr<BaseManager> pBase)
{
    if (pBase)
    {
        m_bases.push_back(std::move(pBase));
    }
}

BaseManager* Faction::CreateBase(FactionId factionId, int baseId, const std::string& name, int x, int y,
                                  const GameDataContext& rDataContext,
                                  const WorldMap& rWorldMap)
{
    auto pBase = std::make_unique<BaseManager>(
        rDataContext.buildingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popCompositionCalculator.get(),
        rWorldMap);
    pBase->SetFactionId(factionId);
    pBase->SetBaseId(baseId);
    pBase->SetName(name);
    pBase->SetPosition(x, y);

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
            pBase->CollectResources(m_pEconomy.get());
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

std::vector<const PopTypeConfig*> Faction::GetAvailablePopTypes() const
{
    if (!m_pPopTypeRegistry || !m_pResearch)
    {
        throw std::runtime_error("Faction::GetAvailablePopTypes: Missing registry or research manager");
    }

    std::vector<const PopTypeConfig*> pAvailable;
    for (const PopTypeConfig& rConfig : m_pPopTypeRegistry->GetAll())
    {
        if (!rConfig.bPlayerAssignable)
        {
            continue;
        }

        if (!rConfig.requiredTech.empty() && !m_pResearch->HasDiscoveredTech(rConfig.requiredTech))
        {
            continue;
        }

        pAvailable.push_back(&rConfig);
    }
    return pAvailable;
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
