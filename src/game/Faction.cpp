#include "game/Faction.h"

#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/GameDataContext.h"
#include "game/map/WorldMap.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include <iostream>
#include <stdexcept>
#include "game/faction/AIProfile.h"
#include "game/faction/FactionFlavor.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/ResearchSelector.h"
#include "game/faction/Diplomacy.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/Pop.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/units/Unit.h"
#include "game/effects/ActiveEffect.h"

namespace ac
{

Faction::Faction(FactionId factionId, bool bIsPlayerControlled,
                 const FactionConfig_t& rDefinition,
                 const BuildingRegistry* pBuildingRegistry, const TechRegistry* pTechRegistry,
                 const SocialPolicyRegistry* pSocialPolicyRegistry,
                 const SocialRatingRegistry* pSocialRatingRegistry,
                 TechCostCalculator* pTechCostCalculator,
                 const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator)
    : m_factionId(factionId)
    , m_bIsPlayerControlled(bIsPlayerControlled)
    , m_rDefinition(rDefinition)
    , m_pBuildingRegistry(pBuildingRegistry)
    , m_pPopTypeAvailabilityCalculator(pPopTypeAvailabilityCalculator)
    , m_pIdentity(std::make_unique<FactionIdentity>(rDefinition.identity, rDefinition.leader))
    , m_pAIProfile(std::make_unique<AIProfile>(rDefinition.ai))
    , m_pFlavor(std::make_unique<FactionFlavor>(rDefinition.flavor, *m_pIdentity))
    , m_pEconomy(std::make_unique<EconomyManager>())
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(pTechRegistry, pTechCostCalculator, this))
    , m_pResearchSelector(std::make_unique<ResearchSelector>(m_pResearch.get()))
    , m_pDiplomacy(nullptr)
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(pSocialPolicyRegistry,
                                                                        pSocialRatingRegistry))
    , m_pUnits(std::make_unique<UnitManager>(*this))
    , m_effectsPool(pBuildingRegistry, m_baseListRevision)
{
    m_pResearchSelector->EnsureResearchTarget();
}

Faction::~Faction()
{
}

std::string Faction::SuggestBaseName()
{
    return m_pFlavor->PickBaseName();
}

EconomyManager& Faction::GetEconomy()
{
    return *m_pEconomy;
}

const EconomyManager& Faction::GetEconomy() const
{
    return *m_pEconomy;
}

int Faction::CollectIncome()
{
    int total = 0;
    for (const auto& pBase : m_bases)
    {
        total += pBase->GetResources().ConsumeEcon();
    }
    m_pEconomy->AddEnergy(total);
    return total;
}

int Faction::CollectResearch()
{
    int total = 0;
    for (const auto& pBase : m_bases)
    {
        total += pBase->GetResources().ConsumeLabs();
    }
    m_pResearch->AddResearchPoints(total);
    return total;
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

std::optional<int> Faction::GetBreakthroughRate() const
{
    return m_pResearch->BreakthroughRate(GetResearchPerTurn_());
}

std::optional<int> Faction::GetTurnsUntilBreakthrough() const
{
    return m_pResearch->GetTurnsUntilBreakthrough(GetResearchPerTurn_());
}

void Faction::AddBase(std::unique_ptr<BaseManager> pBase)
{
    if (!pBase)
    {
        throw std::invalid_argument("Faction::AddBase: pBase is null");
    }
    m_bases.push_back(std::move(pBase));
    m_baseListRevision.Bump();
    RebuildVisibility();
}

BaseManager* Faction::CreateBase(int baseId, const std::string& name, Tile* pTile,
                                  const GameDataContext& rDataContext,
                                  TileEffectsContext& rTileEffects,
                                  const SecretProjectAvailabilityCalculator& rSecretProjectAvailability)
{
    if (!pTile)
    {
        throw std::invalid_argument("Faction::CreateBase: pTile is null");
    }
    auto pBase = std::make_unique<BaseManager>(
        m_factionId, baseId, name, *pTile,
        rDataContext.buildingRegistry.get(),
        rDataContext.socialRatingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popTypeAvailabilityCalculator.get(),
        rDataContext.growthConfig.get(),
        rDataContext.popCompositionCalculator.get(),
        &rSecretProjectAvailability,
        rTileEffects,
        m_pResearch.get(), m_pEconomy.get(), this);

    pBase->GetWorkerAssignments().UnassignAll();
    pBase->GetWorkerAssignments().AutoAssignWorkers();

    std::cout << "Created base '" << name << "' with population: " << pBase->GetPopulation().GetSize()
              << " (workers: " << pBase->GetPopulation().GetWorkerCount() << ")\n";

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));
    return pRawBase;
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

Military& Faction::GetMilitary()
{
    return *m_pMilitary;
}

const Military& Faction::GetMilitary() const
{
    return *m_pMilitary;
}

ResearchManager& Faction::GetResearch()
{
    return *m_pResearch;
}

const ResearchManager& Faction::GetResearch() const
{
    return *m_pResearch;
}

bool Faction::DiscoverCurrentResearch()
{
    if (!m_pResearch->DiscoverTech())
    {
        return false;
    }

    m_pResearchSelector->EnsureResearchTarget();
    return true;
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
        if (rConfig.IsAvailable(discoveredTechs))
        {
            discovered.push_back(&rConfig);
        }
    }
    return discovered;
}

SocialEngineeringManager& Faction::GetSocialEngineering()
{
    return *m_pSocialEngineering;
}

const SocialEngineeringManager& Faction::GetSocialEngineering() const
{
    return *m_pSocialEngineering;
}

UnitManager& Faction::GetUnitManager()
{
    return *m_pUnits;
}

const UnitManager& Faction::GetUnitManager() const
{
    return *m_pUnits;
}

FactionVisibilityMap& Faction::GetVisibility()
{
    return m_visibility;
}

const FactionVisibilityMap& Faction::GetVisibility() const
{
    return m_visibility;
}

void Faction::BindWorldMap(WorldMap& rWorldMap)
{
    m_pWorldMap = &rWorldMap;
    m_visibility.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    RebuildVisibility();
}

void Faction::RebuildVisibility()
{
    if (!m_pWorldMap)
    {
        return;
    }
    m_visibility.RebuildFromSources(*this, *m_pWorldMap);
}

std::vector<const PopTypeConfig_t*> Faction::GetAvailablePopTypes() const
{
    if (!m_pPopTypeAvailabilityCalculator || !m_pResearch)
    {
        throw std::runtime_error("Faction::GetAvailablePopTypes: Missing calculator or research manager");
    }

    return m_pPopTypeAvailabilityCalculator->GetAvailable(m_pResearch->GetDiscoveredTechs());
}

const FactionEffects_t& Faction::GetActiveEffects() const
{
    return m_effectsPool.Get(*this);
}

uint64_t Faction::GetEffectsVersion() const
{
    return m_effectsPool.GetVersion(*this);
}

} // namespace ac
