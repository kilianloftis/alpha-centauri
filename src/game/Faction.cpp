#include "game/Faction.h"

#include "game/buildings/BuildingRegistry.h"
#include "game/GameDataContext.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/map/WorldMap.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include <iostream>
#include <stdexcept>
#include "game/faction/AIProfile.h"
#include "game/faction/FactionFlavor.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/Diplomacy.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/Pop.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/units/Unit.h"
#include "lib/effects/ActiveEffect.h"

namespace ac
{

Faction::Faction(const BuildingRegistry* pBuildingRegistry, const TechRegistry* pTechRegistry,
                 const SocialPolicyRegistry* pSocialPolicyRegistry,
                 const SocialRatingRegistry* pSocialRatingRegistry,
                 TechCostCalculator* pTechCostCalculator,
                 const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
                 const FactionConfig_t* pDefinition)
    : m_pDefinition(pDefinition)
    , m_pBuildingRegistry(pBuildingRegistry)
    , m_pPopTypeAvailabilityCalculator(pPopTypeAvailabilityCalculator)
    , m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pFlavor(nullptr)
    , m_pEconomy(std::make_unique<EconomyManager>())
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(pTechRegistry, pTechCostCalculator))
    , m_pDiplomacy(nullptr)
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(pSocialPolicyRegistry,
                                                                        pSocialRatingRegistry))
    , m_pUnits(std::make_unique<UnitManager>(*this))
{
    if (m_pResearch)
    {
        m_pResearch->SetEffectsProvider(this);
    }
    if (m_pDefinition)
    {
        m_pIdentity = std::make_unique<FactionIdentity>(m_pDefinition->identity, m_pDefinition->leader);

        m_pAIProfile = std::make_unique<AIProfile>(m_pDefinition->ai);
        m_pFlavor = std::make_unique<FactionFlavor>(m_pDefinition->flavor, *m_pIdentity);
    }
}

Faction::~Faction()
{
}

const std::string& Faction::GetDefinitionId() const
{
    static const std::string kEmpty;
    return m_pDefinition ? m_pDefinition->id : kEmpty;
}

std::string Faction::SuggestBaseName()
{
    if (!m_pFlavor)
    {
        throw std::runtime_error("Faction has no flavor configuration");
    }
    return m_pFlavor->PickBaseName();
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

int Faction::GetNetIncomePerTurn() const
{
    int total = 0;
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            total += pBase->GetEconProduction();
        }
    }
    return total;
}

int Faction::GetResearchPerTurn_() const
{
    int total = 0;
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            total += pBase->GetLabsProduction();
        }
    }
    return total;
}

int Faction::GetBreakthroughRate() const
{
    if (!m_pResearch)
    {
        return -1;
    }

    return m_pResearch->BreakthroughRate(GetResearchPerTurn_());
}

int Faction::GetTurnsUntilBreakthrough() const
{
    if (!m_pResearch)
    {
        return -1;
    }

    return m_pResearch->GetTurnsUntilBreakthrough(GetResearchPerTurn_());
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
        *pTile, rDataContext, rTileEffects, m_pResearch.get(), m_pEconomy.get(), this);
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

void Faction::ProduceBaseResources(const std::vector<ActiveEffect_t>& rExternalEffects)
{
    FactionEffects_t factionEffects = GetActiveEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), rExternalEffects.begin(), rExternalEffects.end());

    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ProduceResources(factionEffects);
        }
    }
}

void Faction::ApplyBaseGrowth(const std::vector<ActiveEffect_t>& rExternalEffects)
{
    FactionEffects_t factionEffects = GetActiveEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), rExternalEffects.begin(), rExternalEffects.end());

    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ApplyGrowth(factionEffects);
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

std::vector<ActiveEffect_t> Faction::CollectDefinitionEffects() const
{
    std::vector<ActiveEffect_t> result;
    if (m_pDefinition)
    {
        AppendActiveEffects(m_pDefinition->effects, nullptr, m_pDefinition->id, result);
    }
    return result;
}

std::vector<ActiveEffect_t> Faction::CollectPopFactionEffects() const
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pBase : m_bases)
    {
        if (!pBase)
        {
            continue;
        }
        for (const auto& pPop : pBase->GetPopContainer().GetPops())
        {
            if (!pPop)
            {
                continue;
            }
            // ThisPop is resolved by the pop itself; ThisBase merges per base via
            // CollectFromPops. Only faction-lane scopes enter the pool.
            AppendFactionLaneEffects(pPop->GetConfig().effects, pPop->GetConfig().id, result);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> Faction::CollectUnitFactionEffects() const
{
    std::vector<ActiveEffect_t> result;
    if (!m_pUnits)
    {
        return result;
    }
    for (const auto& pUnit : m_pUnits->GetUnits())
    {
        if (!pUnit)
        {
            continue;
        }
        for (const ActiveEffect_t& rEffect : pUnit->GetDesign().CollectEffects())
        {
            // ThisUnit is resolved by the unit itself; ThisTile by the tile resolvers
            // scanning units on the map. Only faction-lane scopes enter the pool.
            if (IsFactionLane(rEffect.config->scope))
            {
                result.push_back(rEffect);
            }
        }
    }
    return result;
}

UnitManager& Faction::GetUnitManager()
{
    return *m_pUnits;
}

const UnitManager& Faction::GetUnitManager() const
{
    return *m_pUnits;
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

FactionEffects_t Faction::GetActiveEffects() const
{
    FactionEffects_t factionEffects;
    const std::vector<ActiveEffect_t> defEffects = CollectDefinitionEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), defEffects.begin(), defEffects.end());

    const std::vector<ActiveEffect_t> buildingEffects = CollectBuildingEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), buildingEffects.begin(), buildingEffects.end());

    const std::vector<ActiveEffect_t> seEffects = CollectSocialEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), seEffects.begin(), seEffects.end());

    const std::vector<ActiveEffect_t> popEffects = CollectPopFactionEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), popEffects.begin(), popEffects.end());

    const std::vector<ActiveEffect_t> unitEffects = CollectUnitFactionEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), unitEffects.begin(), unitEffects.end());

    return factionEffects;
}

} // namespace ac
