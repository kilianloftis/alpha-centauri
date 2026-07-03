#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
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
    const BuildingRegistry* pBuildingRegistry,
    const PopTypeRegistry* pPopRegistry,
    const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
    PopCompositionCalculator* pCompositionCalculator,
    TileEffectsContext& rTileEffects,
    const ResearchManager* pResearchManager,
    const EconomyManager* pEconomyManager,
    const ProductionCostCalculator* pProductionCostCalculator,
    const GrowthConfig_t& rGrowthConfig,
    const SecretProjectAvailabilityCalculator* pSecretProjectCalculator)
    : m_factionId(-1)
    , m_baseId(-1)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_pBuildingRegistry(pBuildingRegistry)
    , m_pResearch(pResearchManager)
    , m_pPopulation(std::make_unique<PopulationManager>(pPopRegistry, pPopTypeAvailabilityCalculator, pResearchManager, pCompositionCalculator, rGrowthConfig, 3))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(ComputeWorkableTiles_(rTileEffects, tile), m_pPopulation->GetContainer(), rTileEffects))
    , m_pResources(nullptr)
    , m_pBuildings(std::make_unique<BuildingManager>(pBuildingRegistry, pResearchManager, pSecretProjectCalculator))
    , m_pProduction(pProductionCostCalculator ? std::make_unique<ProductionManager>(*pProductionCostCalculator) : nullptr)
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

void BaseManager::RecalculatePopComposition()
{
    if (m_pPopulation)
    {
        m_pPopulation->RecalculateComposition();
    }
}

const PopContainer& BaseManager::GetPopContainer() const
{
    return m_pPopulation->GetContainer();
}

TileEffectsContext& BaseManager::GetTileEffects()
{
    return m_rTileEffects;
}

const TileEffectsContext& BaseManager::GetTileEffects() const
{
    return m_rTileEffects;
}

int BaseManager::GetPopWorkerCount() const
{
    return m_pPopulation ? m_pPopulation->GetWorkerCount() : 0;
}

void BaseManager::ConvertPop(Pop& rPop, const std::string& typeId)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::ConvertPop: m_pPopulation is null");
    }
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

const std::string& BaseManager::GetDefaultWorkerTypeId() const
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::GetDefaultWorkerTypeId: m_pPopulation is null");
    }
    return m_pPopulation->GetDefaultPopType();
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
    m_pWorkerAssignments->UserAssignBestAvailableWorker(pTile, GetDefaultWorkerTypeId());
}

void BaseManager::AutoAssignWorkers()
{
    m_pWorkerAssignments->AutoAssignWorkers();
}

int BaseManager::GetNutrientProduction() const
{
    return m_pResources ? m_pResources->GetNutrientProduction() : 0;
}

int BaseManager::GetMineralProduction() const
{
    return m_pResources ? m_pResources->GetMineralProduction() : 0;
}

int BaseManager::GetEconProduction() const
{
    return m_pResources ? m_pResources->GetEconProduction() : 0;
}

int BaseManager::GetLabsProduction() const
{
    return m_pResources ? m_pResources->GetLabsProduction() : 0;
}

int BaseManager::GetPsychProduction() const
{
    return m_pResources ? m_pResources->GetPsychProduction() : 0;
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

void BaseManager::ProduceResources(const std::vector<ActiveEffect_t>& activeEffects)
{
    if (!m_pResources || !m_pPopulation)
    {
        throw std::runtime_error("BaseManager::ProduceResources: m_pResources or m_pPopulation is null");
    }

    std::vector<ActiveEffect_t> baseEffects = FilterForBase(activeEffects, *this);

    const std::vector<ActiveEffect_t> popEffects = CollectFromPops(GetPopContainer(), *this);
    baseEffects.insert(baseEffects.end(), popEffects.begin(), popEffects.end());

    m_pResources->ProduceResources(baseEffects);
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

void BaseManager::ApplyGrowth(const std::vector<ActiveEffect_t>& activeEffects)
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::ApplyGrowth: m_pPopulation is null");
    }
    const int nutrients = m_pResources ? m_pResources->ConsumeNutrients() : 0;
    const std::vector<ActiveEffect_t> baseEffects = FilterForBase(activeEffects, *this);
    m_pPopulation->ApplyGrowth(nutrients, baseEffects);
}

int BaseManager::GetNutrientStockpile() const
{
    return m_pPopulation ? m_pPopulation->GetNutrientStockpile() : 0;
}

int BaseManager::GetNutrientsRequired(const std::vector<ActiveEffect_t>& activeEffects) const
{
    if (!m_pPopulation)
    {
        throw std::runtime_error("BaseManager::GetNutrientsRequired: m_pPopulation is null");
    }
    const std::vector<ActiveEffect_t> baseEffects = FilterForBase(activeEffects, *this);
    return m_pPopulation->GetNutrientsRequired(baseEffects);
}

int BaseManager::GetBaseSize() const
{
    return m_pPopulation ? m_pPopulation->GetSize() : 0;
}

int BaseManager::GetGrowthRate() const
{
    // TODO: Implement growth rate calculation
    // For now, return arbitrary value as placeholder
    return 0;
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
