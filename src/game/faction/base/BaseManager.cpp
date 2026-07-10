#include "game/faction/base/BaseManager.h"
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
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include <stdexcept>

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

WorkedTileClaim ClaimCenterTile_(TileEffectsContext& rTileEffects, const Tile& tile)
{
    // The base tile is worked for free by this base alone, so it is claimed in the world
    // index for the base's lifetime — another base can never work it. A worker currently
    // on the tile is displaced, and its own base auto-reassigns it to the best free tile
    // in its radius. Throws only if the tile is another base's own tile, which a founding
    // flow must never allow.
    return rTileEffects.GetWorldMap().GetWorkedTiles().ClaimDisplacing(tile, /*bUserAssigned*/false);
}

} // namespace

BaseManager::BaseManager(
    FactionId factionId,
    int baseId,
    std::string name,
    Tile& tile,
    const BuildingRegistry* pBuildingRegistry,
    const SocialRatingRegistry* pSocialRatingRegistry,
    const PopTypeRegistry* pPopTypeRegistry,
    const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
    const GrowthConfig_t* pGrowthConfig,
    PopCompositionCalculator* pCompositionCalculator,
    const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
    TileEffectsContext& rTileEffects,
    const ResearchManager* pResearchManager,
    const EconomyManager* pEconomyManager,
    const IEffectsProvider* pEffectsProvider)
    : m_factionId(factionId)
    , m_baseId(baseId)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_centerTileClaim(ClaimCenterTile_(rTileEffects, tile))
    , m_pBuildingRegistry(pBuildingRegistry)
    , m_pSocialRatings(pSocialRatingRegistry)
    , m_pResearch(pResearchManager)
    , m_pEffectsProvider(pEffectsProvider)
    , m_pPopulation(std::make_unique<PopulationManager>(
          pPopTypeRegistry, pPopTypeAvailabilityCalculator, pGrowthConfig, pCompositionCalculator,
          pResearchManager, 3))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(ComputeWorkableTiles_(rTileEffects, tile), *m_pPopulation, rTileEffects, rTileEffects.GetWorldMap().GetWorkedTiles()))
    , m_pBuildings(std::make_unique<BuildingManager>(pBuildingRegistry, pSecretProjectCalculator, pResearchManager))
    , m_pResources(std::make_unique<ResourceManager>(
          m_pWorkerAssignments.get(), pEconomyManager, m_pBuildings.get(), &m_tile, &m_rTileEffects))
    , m_pProduction(std::make_unique<ProductionManager>())
    , m_name(std::move(name))
{
    // A base provides its own garrison defense bonus, modeled as the "Base" improvement.
    m_rTileEffects.AddImprovementWithEffects(m_tile, "Base");

    m_pPopulation->OnGrowth.Connect([this]() {
        m_pPopulation->AddPop();
    });
    m_pPopulation->OnStarvation.Connect([this]() {
        m_pPopulation->RemovePop();
    });
    m_pPopulation->OnPopGained.Connect([this](int newSize) {
        m_pWorkerAssignments->AutoAssignWorkers();
        OnPopGained.Emit(newSize);
    });
    // Newly created pops start unassigned; auto-assign once after construction.
    m_pWorkerAssignments->AutoAssignWorkers();
    m_pPopulation->OnPopLost.Connect([this](int newSize) {
        OnPopLost.Emit(newSize);
    });

    m_pProduction->OnProductionCompleted.Connect([this](const std::string& itemId) {
        m_pBuildings->AddBuilding(itemId);
        if (!m_pBuildingRegistry)
        {
            throw std::runtime_error("BaseManager: building registry is null after production");
        }
        DispatchInstantaneousEffects(m_pBuildingRegistry->Get(itemId), *this);
        OnProductionCompleted.Emit(itemId);
    });
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
    return m_pResources->GetNutrientProduction(BuildBaseEffects_());
}

int BaseManager::GetMineralProduction() const
{
    return m_pResources->GetMineralProduction(BuildBaseEffects_());
}

int BaseManager::GetEconProduction() const
{
    return m_pResources->GetEconProduction(BuildBaseEffects_());
}

int BaseManager::GetLabsProduction() const
{
    return m_pResources->GetLabsProduction(BuildBaseEffects_());
}

int BaseManager::GetPsychProduction() const
{
    return m_pResources->GetPsychProduction(BuildBaseEffects_());
}

BuildingManager& BaseManager::GetBuildingManager()
{
    return *m_pBuildings;
}

const BuildingManager& BaseManager::GetBuildingManager() const
{
    return *m_pBuildings;
}

std::vector<ActiveEffect_t> BaseManager::CollectBuildingEffects() const
{
    std::vector<ActiveEffect_t> result = m_pBuildings->CollectEffects();

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
    for (const BuildingConfig_t* pBuilding : m_pBuildings->GetBuildingsAvailableForConstruction())
    {
        available.push_back(pBuilding);
    }
    return available;
}

ProductionManager& BaseManager::GetProduction()
{
    return *m_pProduction;
}

const ProductionManager& BaseManager::GetProduction() const
{
    return *m_pProduction;
}

std::string BaseManager::ApplyProduction()
{
    return m_pProduction->ApplyProduction(m_pResources->ConsumeMinerals(), BuildBaseEffects_());
}

int BaseManager::GetMineralCost() const
{
    return m_pProduction->GetMineralCost(BuildBaseEffects_());
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

const BaseEffects_t& BaseManager::BuildBaseEffects_() const
{
    if (!m_pEffectsProvider)
    {
        throw std::runtime_error("BaseManager::BuildBaseEffects_: m_pEffectsProvider is null");
    }
    const FactionEffects_t& rPool = m_pEffectsProvider->GetActiveEffects();
    const uint64_t poolVersion = m_pEffectsProvider->GetEffectsVersion();
    if (poolVersion != m_cachedPoolVersion)
    {
        m_cachedBaseEffects = BuildBaseEffects_(rPool);
        m_cachedPoolVersion = poolVersion;
    }
    return m_cachedBaseEffects;
}

int BaseManager::GetEffectiveSocialRating(SocialRatingId_t rating) const
{
    if (!m_pEffectsProvider)
    {
        throw std::runtime_error("BaseManager::GetEffectiveSocialRating: m_pEffectsProvider is null");
    }
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(CollectBaseLocalEffects_(m_pEffectsProvider->GetActiveEffects()));
    const auto it = totals.find(rating);
    return it == totals.end() ? 0 : it->second;
}

void BaseManager::ProduceResources(const FactionEffects_t& rFactionEffects)
{
    m_pResources->ProduceResources(BuildBaseEffects_(rFactionEffects));
}

ResourceManager& BaseManager::GetResources()
{
    return *m_pResources;
}

const ResourceManager& BaseManager::GetResources() const
{
    return *m_pResources;
}

void BaseManager::ApplyGrowth(const FactionEffects_t& rFactionEffects)
{
    m_pPopulation->ApplyGrowth(m_pResources->ConsumeNutrients(), BuildBaseEffects_(rFactionEffects));
}

int BaseManager::GetNutrientsRequired() const
{
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

TileResources_t BaseManager::GetWorkedTileYield(const Tile& rTile) const
{
    return m_pWorkerAssignments->GetWorkedTileYield(rTile, BuildBaseEffects_());
}

const std::string& BaseManager::GetName() const
{
    return m_name;
}

FactionId BaseManager::GetFactionId() const
{
    return m_factionId;
}

int BaseManager::GetBaseId() const
{
    return m_baseId;
}

} // namespace ac
