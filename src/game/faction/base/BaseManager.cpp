#include "game/faction/base/BaseManager.h"
#include "game/Faction.h"
#include "game/GameState.h"
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
    Faction& rFaction,
    BaseId_t baseId,
    std::string name,
    Tile& tile,
    const BuildingRegistry& rBuildingRegistry,
    const SocialRatingRegistry& rSocialRatingRegistry,
    const PopTypeRegistry& rPopTypeRegistry,
    const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
    const GrowthConfig_t& rGrowthConfig,
    PopCompositionCalculator& rCompositionCalculator,
    const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
    TileEffectsContext& rTileEffects,
    int initialPopulation)
    : m_pFaction(&rFaction)
    , m_baseId(baseId)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_centerTileClaim(ClaimCenterTile_(rTileEffects, tile))
    , m_homeUnits(*this)
    , m_rBuildingRegistry(rBuildingRegistry)
    , m_rSocialRatings(rSocialRatingRegistry)
    , m_pEffectsProvider(&rFaction)
    , m_pPopulation(std::make_unique<PopulationManager>(
          rPopTypeRegistry, rPopTypeAvailabilityCalculator, rGrowthConfig, rCompositionCalculator,
          rFaction.GetResearch(), initialPopulation))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(
          ComputeWorkableTiles_(rTileEffects, tile), *m_pPopulation, rTileEffects,
          rTileEffects.GetWorldMap().GetWorkedTiles()))
    , m_pBuildings(std::make_unique<BuildingManager>(rBuildingRegistry, pSecretProjectCalculator,
                                                     rFaction.GetResearch()))
    , m_pResources(std::make_unique<ResourceManager>(
          *m_pWorkerAssignments, rFaction.GetEconomy(), m_tile, m_rTileEffects, m_homeUnits))
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
        // Checked before the base is mutated: a throw here must not leave the building
        // constructed with its Instantaneous effects never dispatched.
        GameState* pGameState = m_pFaction->GetGameState();
        if (!pGameState)
        {
            throw std::runtime_error(
                "BaseManager: Faction has no GameState bound; cannot dispatch Instantaneous effects");
        }
        m_pBuildings->AddBuilding(itemId);
        DispatchInstantaneousEffects(m_rBuildingRegistry.Get(itemId), *this, *pGameState);
        OnProductionCompleted.Emit(itemId);
    });
}

BaseManager::~BaseManager()
{
    // Emitted first, while every member is still fully alive, so an observer (BaseView's
    // pop-on-destroy) can drop its reference before the object actually goes away — mirrors
    // UnitManager::OnUnitDestroyed's "signal before erase" contract.
    OnDestroyed.Emit();
    m_rTileEffects.RemoveImprovementWithEffects(m_tile, "Base");
}

BaseSnapshot_t BaseManager::CaptureSnapshot() const
{
    BaseSnapshot_t snapshot;
    snapshot.baseId = m_baseId;
    snapshot.name = m_name;
    snapshot.pTile = &m_tile;
    snapshot.populationSize = m_pPopulation->GetSize();
    for (const BuildingConfig_t* pBuilding : m_pBuildings->GetBuildings())
    {
        if (pBuilding)
        {
            snapshot.buildingIds.push_back(pBuilding->id);
        }
    }
    if (const IConstructable* pProduction = m_pProduction->GetCurrentProduction())
    {
        snapshot.productionItemId = pProduction->GetId();
    }
    snapshot.mineralStockpile = m_pProduction->GetMineralStockpile();
    snapshot.nutrientStockpile = m_pPopulation->GetNutrientStockpile();
    return snapshot;
}

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
    m_pWorkerAssignments->UserAssignBestAvailableWorker(pTile);
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
    return m_pBuildings->CollectEffects(*this);
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

BaseEffects_t BaseManager::CollectRatingSource_() const
{
    return CollectBaseLocalEffects_(m_pEffectsProvider->GetLocalActiveEffects());
}

BaseEffects_t BaseManager::BuildBaseEffects_(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = CollectBaseLocalEffects_(rFactionEffects);

    // Ratings are a faction-internal axis: accumulate from the local pool only, then append
    // the level effects onto the composed base list (which may also carry world/council
    // StatModifiers). The two lists differ, which is why the resolver returns its effects
    // instead of expanding in place.
    const std::vector<ActiveEffect_t> ratingEffects =
        ResolveSocialRatingLevelEffects(CollectRatingSource_(), m_rSocialRatings);
    baseEffects.effects.insert(baseEffects.effects.end(), ratingEffects.begin(),
                               ratingEffects.end());

    return baseEffects;
}

const BaseEffects_t& BaseManager::BuildBaseEffects_() const
{
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
    // Local-only ratings: peer WorldGlobal / council SocialRatingModifiers never move this
    // axis (see AccumulateSocialRatings). Same source list as the base-lane expansion.
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(CollectRatingSource_().effects);
    const auto it = totals.find(rating);
    return it == totals.end() ? 0 : it->second;
}

void BaseManager::ProduceResources()
{
    m_pResources->ProduceResources(BuildBaseEffects_());
}

ResourceManager& BaseManager::GetResources()
{
    return *m_pResources;
}

const ResourceManager& BaseManager::GetResources() const
{
    return *m_pResources;
}

void BaseManager::ApplyGrowth()
{
    m_pPopulation->ApplyGrowth(m_pResources->ConsumeNutrients(), BuildBaseEffects_());
}

int BaseManager::GetNutrientsRequired() const
{
    return m_pPopulation->GetNutrientsRequired(BuildBaseEffects_());
}

Tile& BaseManager::GetTile()
{
    return m_tile;
}

const Tile& BaseManager::GetTile() const
{
    return m_tile;
}

const BaseEffects_t& BaseManager::GetBaseEffects() const
{
    return BuildBaseEffects_();
}

TileYieldView_t BaseManager::GetWorkedTileYield(const Tile& rTile) const
{
    return m_pWorkerAssignments->GetWorkedTileYield(rTile, BuildBaseEffects_());
}

TileYieldView_t BaseManager::GetPreviewTileYield(const Tile& rTile) const
{
    return m_rTileEffects.ResolveTileYield(rTile, /*bIsBaseTile*/ false, BuildBaseEffects_());
}

const std::string& BaseManager::GetName() const
{
    return m_name;
}

Faction& BaseManager::GetFaction()
{
    return *m_pFaction;
}

const Faction& BaseManager::GetFaction() const
{
    return *m_pFaction;
}

FactionId_t BaseManager::GetFactionId() const
{
    return m_pFaction->GetFactionId();
}

void BaseManager::RebindFaction(Faction& rFaction)
{
    m_pFaction = &rFaction;
    m_pEffectsProvider = &rFaction;
    m_pPopulation->RebindResearch(rFaction.GetResearch());
    m_pBuildings->RebindResearch(rFaction.GetResearch());
    m_pResources->RebindEconomy(rFaction.GetEconomy());
    // The cached BuildBaseEffects_ result was built from the old owner's pool; force a
    // rebuild against the new provider even if version numbers happen to collide.
    m_cachedPoolVersion.reset();
}

int BaseManager::GetBaseId() const
{
    return m_baseId;
}

HomeBaseIndex& BaseManager::GetHomeUnits()
{
    return m_homeUnits;
}

const HomeBaseIndex& BaseManager::GetHomeUnits() const
{
    return m_homeUnits;
}

} // namespace ac
