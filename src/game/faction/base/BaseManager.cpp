#include "game/faction/base/BaseManager.h"
#include "game/GameDataContext.h"
#include "game/IEffectsProvider.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/TileEffectsContext.h"

namespace ac
{

namespace
{

std::vector<const Tile*> ComputeWorkableTiles_(const TileEffectsContext& rTileEffects, const Tile& tile)
{
    std::vector<const Tile*> tiles;
    ForEachTileInWorkableArea(tile, rTileEffects.GetWorldMap(),
        [&tiles](const Tile* pTile)
        {
            tiles.push_back(pTile);
        });
    return tiles;
}

} // namespace

BaseManager::BaseManager(
    Tile& tile,
    const GameDataContext& rDataContext,
    TileEffectsContext& rTileEffects,
    const ResearchManager* pResearchManager,
    const EconomyManager* pEconomyManager,
    const IEffectsProvider* pEffectsProvider)
    : m_factionId(-1)
    , m_baseId(-1)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_pBuildingRegistry(rDataContext.buildingRegistry.get())
    , m_pSocialRatings(rDataContext.socialRatingRegistry.get())
    , m_pResearch(pResearchManager)
    , m_pEffectsProvider(pEffectsProvider)
    , m_pPopulation(std::make_unique<PopulationManager>(rDataContext, pResearchManager, 3))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(ComputeWorkableTiles_(rTileEffects, tile), *m_pPopulation, rTileEffects))
    , m_pResources(nullptr)
    , m_pBuildings(std::make_unique<BuildingManager>(rDataContext, pResearchManager))
    , m_pProduction(rDataContext.productionCostCalculator
                        ? std::make_unique<ProductionManager>(*rDataContext.productionCostCalculator)
                        : nullptr)
{
    // A base provides its own garrison defense bonus, modeled as the "Base" improvement.
    m_rTileEffects.AddImprovementWithEffects(m_tile, "Base");

    // Create ResourceManager after all sub-managers are set up
    m_pResources = std::make_unique<ResourceManager>(
        m_pWorkerAssignments.get(),
        pEconomyManager,
        m_pBuildings.get(),
        &m_tile,
        &m_rTileEffects);

    m_pPopulation->on_growth.connect([this]() {
        m_pPopulation->AddPop();
    });
    m_pPopulation->on_starvation.connect([this]() {
        m_pPopulation->RemovePop();
    });
    m_pPopulation->on_pop_gained.connect([this](int newSize) {
        m_pWorkerAssignments->AutoAssignWorkers();
        on_pop_gained.emit(newSize);
    });
    // Newly created pops start unassigned; auto-assign once after construction.
    m_pWorkerAssignments->AutoAssignWorkers();
    m_pPopulation->on_pop_lost.connect([this](int newSize) {
        on_pop_lost.emit(newSize);
    });

    if (m_pProduction)
    {
        m_pProduction->on_production_completed.connect([this](const std::string& itemId) {
            m_pBuildings->AddBuilding(itemId);
            if (m_pBuildingRegistry)
            {
                const BuildingConfig_t* pConfig = m_pBuildingRegistry->Find(itemId);
                if (pConfig)
                {
                    DispatchInstantaneousEffects(*pConfig, *this);
                }
            }
            on_production_completed.emit(itemId);
        });
    }
}

BaseManager::~BaseManager() = default;

PopulationManager& BaseManager::GetPopulation()
{
    return *m_pPopulation;
}

const PopulationManager& BaseManager::GetPopulation() const
{
    return *m_pPopulation;
}

TileEffectsContext& BaseManager::GetTileEffects()
{
    return m_rTileEffects;
}

const TileEffectsContext& BaseManager::GetTileEffects() const
{
    return m_rTileEffects;
}

void BaseManager::ConvertPop(Pop& rPop, const std::string& typeId)
{
    if (rPop.IsWorker())
    {
        m_pWorkerAssignments->UnassignWorker(rPop);
    }
    m_pPopulation->ConvertTo(rPop, typeId);
    if (rPop.IsWorker())
    {
        m_pWorkerAssignments->AutoAssignWorkers();
    }
}

WorkerAssignmentManager& BaseManager::GetWorkerAssignments()
{
    return *m_pWorkerAssignments;
}

const WorkerAssignmentManager& BaseManager::GetWorkerAssignments() const
{
    return *m_pWorkerAssignments;
}

void BaseManager::UserAssignBestAvailableWorker(const Tile* pTile)
{
    m_pWorkerAssignments->UserAssignBestAvailableWorker(pTile, m_pPopulation->GetDefaultPopType());
}

int BaseManager::GetNutrientProduction() const
{
    if (!m_pResources)
    {
        throw std::runtime_error("BaseManager::GetNutrientProduction: m_pResources is null");
    }
    return m_pResources->GetNutrientProduction(BuildBaseEffects_());
}

int BaseManager::GetMineralProduction() const
{
    if (!m_pResources)
    {
        throw std::runtime_error("BaseManager::GetMineralProduction: m_pResources is null");
    }
    return m_pResources->GetMineralProduction(BuildBaseEffects_());
}

int BaseManager::GetEconProduction() const
{
    if (!m_pResources)
    {
        throw std::runtime_error("BaseManager::GetEconProduction: m_pResources is null");
    }
    return m_pResources->GetEconProduction(BuildBaseEffects_());
}

int BaseManager::GetLabsProduction() const
{
    if (!m_pResources)
    {
        throw std::runtime_error("BaseManager::GetLabsProduction: m_pResources is null");
    }
    return m_pResources->GetLabsProduction(BuildBaseEffects_());
}

int BaseManager::GetPsychProduction() const
{
    if (!m_pResources)
    {
        throw std::runtime_error("BaseManager::GetPsychProduction: m_pResources is null");
    }
    return m_pResources->GetPsychProduction(BuildBaseEffects_());
}

void BaseManager::AddBuilding(const std::string& buildingId)
{
    if (m_pBuildings)
    {
        m_pBuildings->AddBuilding(buildingId);
    }
}

void BaseManager::DestroyBuilding(const std::string& buildingId)
{
    if (m_pBuildings)
    {
        m_pBuildings->DestroyBuilding(buildingId);
    }
}

const std::vector<const BuildingConfig_t*>& BaseManager::GetBuildings() const
{
    return m_pBuildings->GetBuildings();
}

std::vector<ActiveEffect_t> BaseManager::CollectBuildingEffects() const
{
    std::vector<ActiveEffect_t> result = m_pBuildings
        ? m_pBuildings->CollectEffects()
        : std::vector<ActiveEffect_t>{};

    for (ActiveEffect_t& effect : result)
    {
        if (effect.config && effect.config->scope == EffectScope_t::ThisBase)
        {
            effect.originBase = this;
        }
    }
    return result;
}

std::vector<const IConstructable*> BaseManager::GetConstructable() const
{
    std::vector<const IConstructable*> available;
    if (m_pBuildings)
    {
        std::vector<const BuildingConfig_t*> buildings = m_pBuildings->GetBuildingsAvailableForConstruction();
        for (const BuildingConfig_t* pBuilding : buildings)
        {
            available.push_back(pBuilding);
        }
    }
    return available;
}

void BaseManager::SetProduction(const IConstructable* pItem)
{
    if (m_pProduction)
    {
        m_pProduction->SetProduction(pItem);
    }
}

const IConstructable* BaseManager::GetCurrentProduction() const
{
    return m_pProduction ? m_pProduction->GetCurrentProduction() : nullptr;
}

int BaseManager::GetProductionMineralCost() const
{
    return m_pProduction ? m_pProduction->GetMineralCost() : 0;
}

int BaseManager::GetMineralStockpile() const
{
    return m_pProduction ? m_pProduction->GetMineralStockpile() : 0;
}

std::string BaseManager::ApplyProduction()
{
    if (!m_pProduction)
    {
        return std::string();
    }
    const int minerals = m_pResources ? m_pResources->ConsumeMinerals() : 0;
    return m_pProduction->ApplyProduction(minerals);
}

BaseEffects_t BaseManager::CollectBaseLocalEffects_(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = FilterForBase(rFactionEffects, *this);

    const std::vector<ActiveEffect_t> popEffects = CollectFromPops(*m_pPopulation, *this);
    baseEffects.effects.insert(baseEffects.effects.end(), popEffects.begin(), popEffects.end());

    return baseEffects;
}

BaseEffects_t BaseManager::BuildBaseEffects_(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = CollectBaseLocalEffects_(rFactionEffects);

    // Map this base's effective social rating levels (faction-wide modifiers + any
    // ThisBase-scoped ones that survived FilterForBase) to their gameplay effects.
    if (m_pSocialRatings)
    {
        ExpandSocialRatingEffects(baseEffects, *m_pSocialRatings);
    }

    return baseEffects;
}

BaseEffects_t BaseManager::BuildBaseEffects_() const
{
    if (!m_pEffectsProvider)
    {
        throw std::runtime_error("BaseManager::BuildBaseEffects_: m_pEffectsProvider is null");
    }
    return BuildBaseEffects_(m_pEffectsProvider->GetActiveEffects());
}

int BaseManager::GetEffectiveSocialRating(SocialRatingId rating) const
{
    if (!m_pEffectsProvider)
    {
        throw std::runtime_error("BaseManager::GetEffectiveSocialRating: m_pEffectsProvider is null");
    }
    const std::map<SocialRatingId, int> totals =
        AccumulateSocialRatings(CollectBaseLocalEffects_(m_pEffectsProvider->GetActiveEffects()));
    const auto it = totals.find(rating);
    return it == totals.end() ? 0 : it->second;
}

void BaseManager::ProduceResources(const FactionEffects_t& rFactionEffects)
{
    if (!m_pResources || !m_pPopulation)
    {
        throw std::runtime_error("BaseManager::ProduceResources: m_pResources or m_pPopulation is null");
    }

    m_pResources->ProduceResources(BuildBaseEffects_(rFactionEffects));
}

int BaseManager::ConsumeEcon()
{
    return m_pResources ? m_pResources->ConsumeEcon() : 0;
}

int BaseManager::ConsumeLabs()
{
    return m_pResources ? m_pResources->ConsumeLabs() : 0;
}

int BaseManager::ConsumePsych()
{
    return m_pResources ? m_pResources->ConsumePsych() : 0;
}

void BaseManager::ApplyGrowth(const FactionEffects_t& rFactionEffects)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::ApplyGrowth: m_pPopulation is null");
    }
    const int nutrients = m_pResources ? m_pResources->ConsumeNutrients() : 0;
    m_pPopulation->ApplyGrowth(nutrients, BuildBaseEffects_(rFactionEffects));
}

int BaseManager::GetNutrientsRequired() const
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::GetNutrientsRequired: m_pPopulation is null");
    }
    return m_pPopulation->GetNutrientsRequired(BuildBaseEffects_());
}

int BaseManager::GetX() const
{
    return m_tile.GetX();
}

int BaseManager::GetY() const
{
    return m_tile.GetY();
}

const std::vector<const Tile*>& BaseManager::GetWorkableTilePositions() const
{
    return m_pWorkerAssignments->GetWorkableTiles();
}

TileResources_t BaseManager::GetWorkedTileYield(const Tile& rTile) const
{
    return m_pWorkerAssignments->GetWorkedTileYield(rTile, BuildBaseEffects_());
}

void BaseManager::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& BaseManager::GetName() const
{
    return m_name;
}

void BaseManager::SetFactionId(FactionId factionId)
{
    m_factionId = factionId;
}

FactionId BaseManager::GetFactionId() const
{
    return m_factionId;
}

void BaseManager::SetBaseId(int baseId)
{
    m_baseId = baseId;
}

int BaseManager::GetBaseId() const
{
    return m_baseId;
}

} // namespace ac
