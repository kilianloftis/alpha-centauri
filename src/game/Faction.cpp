#include "game/faction/VisibilityRules.h"
#include "game/Faction.h"

#include "game/buildings/BuildingRegistry.h"

#include <algorithm>
#include <cstdint>
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
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/population/pop-types/Pop.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/units/Unit.h"
#include "game/units/MoraleCalculator.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/ActiveEffect.h"

namespace ac
{

Faction::Faction(FactionId_t factionId, bool bIsPlayerControlled,
                 const FactionConfig_t& rDefinition,
                 const GameDataContext& rDataContext)
    : m_factionId(factionId)
    , m_bIsPlayerControlled(bIsPlayerControlled)
    , m_rDefinition(rDefinition)
    , m_rDataContext(rDataContext)
    , m_pIdentity(std::make_unique<FactionIdentity>(rDefinition.identity, rDefinition.leader))
    , m_pAIProfile(std::make_unique<AIProfile>(rDefinition.ai))
    , m_pFlavor(std::make_unique<FactionFlavor>(rDefinition.flavor, *m_pIdentity))
    , m_pEconomy(std::make_unique<EconomyManager>())
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(rDataContext.techRegistry.get(),
                                                    rDataContext.techCostCalculator.get(), this))
    , m_pResearchSelector(std::make_unique<ResearchSelector>(m_pResearch.get()))
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(
          rDataContext.socialPolicyRegistry.get(), rDataContext.socialRatingRegistry.get()))
    , m_pUnits(std::make_unique<UnitManager>(*this, *rDataContext.moraleCalculator))
    , m_effectsPool(rDataContext.buildingRegistry.get(), m_baseListRevision,
                    &rDataContext.tileYieldRules, rDataContext.socialRatingRegistry.get())
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

int Faction::TotalPopulation() const
{
    int total = 0;
    for (const BaseManager& rBase : Bases())
    {
        total += rBase.GetPopulation().GetSize();
    }
    return total;
}

int Faction::CountBuildings(const BuildingId_t& buildingId) const
{
    int total = 0;
    for (const BaseManager& rBase : Bases())
    {
        for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
        {
            if (pBuilding && pBuilding->id == buildingId)
            {
                ++total;
            }
        }
    }
    return total;
}

BaseManager* Faction::FindBaseWithBuilding(const BuildingId_t& buildingId)
{
    return const_cast<BaseManager*>(
        static_cast<const Faction*>(this)->FindBaseWithBuilding(buildingId));
}

const BaseManager* Faction::FindBaseWithBuilding(const BuildingId_t& buildingId) const
{
    for (const BaseManager& rBase : Bases())
    {
        for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
        {
            if (pBuilding && pBuilding->id == buildingId)
            {
                return &rBase;
            }
        }
    }
    return nullptr;
}

const BuildingConfig_t* Faction::FindOwnedBuildingConfig(const BuildingId_t& buildingId) const
{
    for (const BaseManager& rBase : Bases())
    {
        for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
        {
            if (pBuilding && pBuilding->id == buildingId)
            {
                return pBuilding;
            }
        }
    }
    return nullptr;
}

int Faction::CountReadyBuildings(const BuildingId_t& buildingId, int missionYear) const
{
    int deployed = 0;
    for (const BuildingDeploy_t& rDeploy : m_buildingDeploys)
    {
        if (rDeploy.buildingId == buildingId && missionYear < rDeploy.readyMissionYear)
        {
            ++deployed;
        }
    }
    const int total = CountBuildings(buildingId);
    return total > deployed ? total - deployed : 0;
}

void Faction::DeployBuilding(const BuildingId_t& buildingId, int readyMissionYear)
{
    m_buildingDeploys.push_back(BuildingDeploy_t{buildingId, readyMissionYear});
}

void Faction::NotifyBuildingDestroyed(const BuildingId_t& buildingId)
{
    // Prefer dropping an active (still cooling) deploy record so ready count stays coherent.
    auto cooling = std::find_if(m_buildingDeploys.begin(), m_buildingDeploys.end(),
                                [&](const BuildingDeploy_t& rDeploy)
                                {
                                    return rDeploy.buildingId == buildingId;
                                });
    if (cooling != m_buildingDeploys.end())
    {
        m_buildingDeploys.erase(cooling);
    }
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
    if (m_onBaseListChanged)
    {
        m_onBaseListChanged();
    }
    RebuildVisibility();
}

std::optional<BaseSnapshot_t> Faction::ExtractBase(BaseId_t baseId)
{
    for (auto it = m_bases.begin(); it != m_bases.end(); ++it)
    {
        if ((*it)->GetBaseId() == baseId)
        {
            BaseSnapshot_t snapshot = (*it)->CaptureSnapshot();
            m_bases.erase(it);
            m_baseListRevision.Bump();
            if (m_onBaseListChanged)
            {
                m_onBaseListChanged();
            }
            RebuildVisibility();
            return snapshot;
        }
    }
    return std::nullopt;
}

BaseManager* Faction::CreateBaseFromSnapshot(
    const BaseSnapshot_t& rSnapshot,
    const GameDataContext& rDataContext,
    TileEffectsContext& rTileEffects,
    const SecretProjectAvailabilityCalculator& rSecretProjectAvailability)
{
    if (!rSnapshot.pTile)
    {
        throw std::invalid_argument("Faction::CreateBaseFromSnapshot: pTile is null");
    }

    auto pBase = std::make_unique<BaseManager>(
        *this, rSnapshot.baseId, rSnapshot.name, *rSnapshot.pTile,
        rDataContext.buildingRegistry.get(),
        rDataContext.socialRatingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popTypeAvailabilityCalculator.get(),
        rDataContext.growthConfig.get(),
        rDataContext.popCompositionCalculator.get(),
        &rSecretProjectAvailability,
        rTileEffects,
        m_pResearch.get(), m_pEconomy.get(), this,
        rSnapshot.populationSize);

    for (const std::string& buildingId : rSnapshot.buildingIds)
    {
        pBase->GetBuildingManager().AddBuilding(buildingId);
    }

    pBase->GetPopulation().SetNutrientStockpile(rSnapshot.nutrientStockpile);

    if (!rSnapshot.productionItemId.empty())
    {
        if (!rDataContext.buildingRegistry)
        {
            throw std::runtime_error(
                "Faction::CreateBaseFromSnapshot: building registry is null");
        }
        pBase->GetProduction().SetProduction(
            &rDataContext.buildingRegistry->Get(rSnapshot.productionItemId));
    }
    pBase->GetProduction().SetMineralStockpile(rSnapshot.mineralStockpile);

    // Psych (and thus drone/talent targets) may differ under the new owner.
    pBase->GetPopulation().RecalculateComposition();
    pBase->GetWorkerAssignments().UnassignAll();
    pBase->GetWorkerAssignments().AutoAssignWorkers();

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));
    return pRawBase;
}

void Faction::TransferBaseTo(BaseId_t baseId,
                             Faction& rReceiver,
                             const GameDataContext& rDataContext,
                             TileEffectsContext& rTileEffects,
                             const SecretProjectAvailabilityCalculator& rSecretProjectAvailability)
{
    if (&rReceiver == this)
    {
        throw std::invalid_argument("Faction::TransferBaseTo: cannot transfer to self");
    }
    std::optional<BaseSnapshot_t> snapshot = ExtractBase(baseId);
    if (!snapshot.has_value())
    {
        throw std::runtime_error("Faction::TransferBaseTo: base not found");
    }
    rReceiver.CreateBaseFromSnapshot(
        *snapshot, rDataContext, rTileEffects, rSecretProjectAvailability);
}

std::optional<UnitSnapshot_t> Faction::ExtractUnit(UnitId_t unitId)
{
    Unit* pFound = nullptr;
    for (Unit& rUnit : GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            pFound = &rUnit;
            break;
        }
    }
    if (!pFound)
    {
        return std::nullopt;
    }
    UnitSnapshot_t snapshot = pFound->CaptureSnapshot();
    GetUnitManager().DestroyUnit(*pFound);
    return snapshot;
}

Unit& Faction::CreateUnitFromSnapshot(const UnitSnapshot_t& rSnapshot,
                                      UnitPositionIndex& rPositions)
{
    if (!rSnapshot.pDesign)
    {
        throw std::invalid_argument("Faction::CreateUnitFromSnapshot: pDesign is null");
    }
    if (!rSnapshot.pTile)
    {
        throw std::invalid_argument("Faction::CreateUnitFromSnapshot: pTile is null");
    }

    Unit& rNew = GetUnitManager().CreateUnit(rSnapshot.unitId, *rSnapshot.pDesign, rPositions,
                                             *rSnapshot.pTile, /*pHomeBase=*/nullptr);
    rNew.SetCurrentHp(rSnapshot.currentHp);
    rNew.SetCurrentFuel(rSnapshot.currentFuel);
    rNew.SetXp(rSnapshot.xp);
    rNew.SetMoveFragmentsRemaining(rSnapshot.moveFragmentsRemaining);
    rNew.ClearOrder();
    return rNew;
}

void Faction::TransferUnitTo(UnitId_t unitId,
                             Faction& rReceiver,
                             UnitPositionIndex& rPositions)
{
    if (&rReceiver == this)
    {
        throw std::invalid_argument("Faction::TransferUnitTo: cannot transfer to self");
    }
    std::optional<UnitSnapshot_t> snapshot = ExtractUnit(unitId);
    if (!snapshot.has_value())
    {
        throw std::runtime_error("Faction::TransferUnitTo: unit not found");
    }
    rReceiver.CreateUnitFromSnapshot(*snapshot, rPositions);
}

BaseManager* Faction::GetHeadquarters()
{
    for (BaseManager& rBase : Bases())
    {
        if (ResolveFlag(rBase, RuleFlagId_t::Headquarters))
        {
            return &rBase;
        }
    }
    return nullptr;
}

const BaseManager* Faction::GetHeadquarters() const
{
    for (const BaseManager& rBase : Bases())
    {
        if (ResolveFlag(rBase, RuleFlagId_t::Headquarters))
        {
            return &rBase;
        }
    }
    return nullptr;
}

BaseManager* Faction::CreateBase(BaseId_t baseId, const std::string& name, Tile* pTile,
                                  const GameDataContext& rDataContext,
                                  TileEffectsContext& rTileEffects,
                                  const SecretProjectAvailabilityCalculator& rSecretProjectAvailability)
{
    if (!pTile)
    {
        throw std::invalid_argument("Faction::CreateBase: pTile is null");
    }
    auto pBase = std::make_unique<BaseManager>(
        *this, baseId, name, *pTile,
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

void Faction::ProduceBaseResources()
{
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ProduceResources();
        }
    }
}

void Faction::ApplyBaseGrowth()
{
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->ApplyGrowth();
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
    if (!m_rDataContext.buildingRegistry || !m_pResearch)
    {
        throw std::runtime_error("BuildingRegistry or ResearchManager not initialized");
    }

    const std::vector<std::string>& discoveredTechs = m_pResearch->GetDiscoveredTechs();

    std::vector<const BuildingConfig_t*> discovered;
    for (const BuildingConfig_t& rConfig : m_rDataContext.buildingRegistry->GetAll())
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

FactionExploredMap& Faction::GetExploredMap()
{
    return m_explored;
}

const FactionExploredMap& Faction::GetExploredMap() const
{
    return m_explored;
}

FactionVisibleMap& Faction::GetVisibleMap()
{
    return m_visible;
}

const FactionVisibleMap& Faction::GetVisibleMap() const
{
    return m_visible;
}

FactionRevealedUnits& Faction::GetRevealedUnits()
{
    return m_revealedUnits;
}

const FactionRevealedUnits& Faction::GetRevealedUnits() const
{
    return m_revealedUnits;
}

void Faction::BindWorldMap(WorldMap& rWorldMap)
{
    m_pWorldMap = &rWorldMap;
    const int width = rWorldMap.GetWidth();
    const int height = rWorldMap.GetHeight();
    m_explored.Reset(width, height);
    m_visible.Reset(width, height);
    RebuildVisibility();
}

void Faction::BindWorldEffects(IWorldEffectsSource& rWorldEffects)
{
    m_pWorldEffects = &rWorldEffects;
    // Force recomposition on next GetActiveEffects.
    m_composedLocalVersion = UINT64_MAX;
    m_composedWorldStamp = UINT64_MAX;
}

void Faction::RebuildVisibility()
{
    if (!m_pWorldMap)
    {
        return;
    }
    m_visible.RebuildFromSources(*this, *m_pWorldMap, m_explored);
    ApplyVisibilityRules(*this, m_pSettings);
    if (m_onVisibilityRebuilt)
    {
        m_onVisibilityRebuilt(*this);
    }
}

void Faction::SetOnBaseListChanged(std::function<void()> handler)
{
    m_onBaseListChanged = std::move(handler);
}

void Faction::SetOnVisibilityRebuilt(std::function<void(Faction&)> handler)
{
    m_onVisibilityRebuilt = std::move(handler);
}

std::vector<const PopTypeConfig_t*> Faction::GetAvailablePopTypes() const
{
    if (!m_rDataContext.popTypeAvailabilityCalculator || !m_pResearch)
    {
        throw std::runtime_error("Faction::GetAvailablePopTypes: Missing calculator or research manager");
    }

    return m_rDataContext.popTypeAvailabilityCalculator->GetAvailable(m_pResearch->GetDiscoveredTechs());
}

const FactionEffects_t& Faction::GetLocalActiveEffects() const
{
    return m_effectsPool.Get(*this);
}

uint64_t Faction::GetLocalEffectsVersion() const
{
    return m_effectsPool.GetVersion(*this);
}

void Faction::EnsureComposedEffects_() const
{
    // GetWorldCompositionStamp sweeps every peer faction's pool, so both public accessors
    // funnel through here rather than each recomputing it — one sweep per query, and the
    // returned list and version can never describe different compositions.
    const uint64_t localVersion = GetLocalEffectsVersion();
    const uint64_t worldStamp =
        m_pWorldEffects ? m_pWorldEffects->GetWorldCompositionStamp(*this) : 0;
    if (localVersion == m_composedLocalVersion && worldStamp == m_composedWorldStamp)
    {
        return;
    }

    m_composedEffects = GetLocalActiveEffects();
    if (m_pWorldEffects)
    {
        const std::vector<ActiveEffect_t> extras = m_pWorldEffects->CollectWorldExtras(*this);
        m_composedEffects.effects.insert(m_composedEffects.effects.end(), extras.begin(),
                                         extras.end());
    }
    m_composedLocalVersion = localVersion;
    m_composedWorldStamp = worldStamp;
    // Monotonic, so consumers comparing versions for equality (BaseManager's base-effect
    // memo, ResearchManager's cost cache) cannot be fooled by a collision the way a hash of
    // (localVersion, worldStamp) could. Starts at 1: never equal to a default-initialized 0.
    ++m_composedVersion;
}

const FactionEffects_t& Faction::GetActiveEffects() const
{
    EnsureComposedEffects_();
    return m_composedEffects;
}

uint64_t Faction::GetEffectsVersion() const
{
    // Moves when the local pool changes *or* when peer WorldGlobal / council inputs do, so
    // downstream memos invalidate on world changes without those effects being folded into
    // the local pool.
    EnsureComposedEffects_();
    return m_composedVersion;
}

} // namespace ac
