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
#include <utility>
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
                 const GameDataContext& rDataContext,
                 WorldMap& rWorldMap,
                 const GameSettings& rSettings,
                 uint32_t seed)
    : m_factionId(factionId)
    , m_bIsPlayerControlled(bIsPlayerControlled)
    , m_rDefinition(rDefinition)
    , m_rDataContext(rDataContext)
    , m_pIdentity(std::make_unique<FactionIdentity>(rDefinition.identity, rDefinition.leader))
    , m_pAIProfile(std::make_unique<AIProfile>(rDefinition.ai))
    // Distinct sub-streams from one seed, so flavor and research picks do not correlate.
    , m_pFlavor(std::make_unique<FactionFlavor>(rDefinition.flavor, *m_pIdentity, seed))
    , m_pEconomy(std::make_unique<EconomyManager>())
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(*rDataContext.techRegistry,
                                                    *rDataContext.techCostCalculator, this))
    , m_pResearchSelector(std::make_unique<ResearchSelector>(*m_pResearch, seed ^ 0x9E3779B9u))
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(
          *rDataContext.socialPolicyRegistry))
    , m_pUnits(std::make_unique<UnitManager>(*this, *rDataContext.moraleCalculator))
    , m_effectsPool(*this, *rDataContext.buildingRegistry, m_baseListRevision,
                    rDataContext.tileYieldRules, *rDataContext.socialRatingRegistry)
    , m_rWorldMap(rWorldMap)
    , m_rSettings(rSettings)
{
    m_pResearchSelector->EnsureResearchTarget();
    // Size the fog maps from the map this faction is bound to for life, then take a first
    // reading. Formerly BindWorldMap's job, which left a window where visibility silently
    // did nothing.
    m_explored.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    m_visible.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    RebuildVisibility();
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
    int cooling = 0;
    for (const BuildingDeploy_t& rDeploy : m_buildingDeploys)
    {
        if (rDeploy.buildingId == buildingId && missionYear < rDeploy.readyMissionYear)
        {
            ++cooling;
        }
    }
    const int total = CountBuildings(buildingId);
    return total > cooling ? total - cooling : 0;
}

void Faction::PruneExpiredDeploys(int missionYear)
{
    std::erase_if(m_buildingDeploys, [missionYear](const BuildingDeploy_t& rDeploy)
    {
        return missionYear >= rDeploy.readyMissionYear;
    });
}

void Faction::DeployBuilding(BaseId_t baseId, const BuildingId_t& buildingId, int readyMissionYear)
{
    m_buildingDeploys.push_back(BuildingDeploy_t{baseId, buildingId, readyMissionYear});
}

void Faction::NotifyBuildingDestroyed(BaseId_t baseId, const BuildingId_t& buildingId)
{
    // Only records for this base: another base's cooling copy of the same buildingId must not
    // be dropped just because this one shares its id. Among this base's records, drop the one
    // with the latest readyMissionYear — the most-cooling copy. A destroyed copy must take a
    // suppression with it, and retiring an already-expired record instead would leave a live
    // cooldown attached to a copy that no longer exists, under-counting the ready total.
    auto latest = m_buildingDeploys.end();
    for (auto it = m_buildingDeploys.begin(); it != m_buildingDeploys.end(); ++it)
    {
        if (it->baseId != baseId || it->buildingId != buildingId)
        {
            continue;
        }
        if (latest == m_buildingDeploys.end() || it->readyMissionYear > latest->readyMissionYear)
        {
            latest = it;
        }
    }
    if (latest != m_buildingDeploys.end())
    {
        m_buildingDeploys.erase(latest);
    }
}

void Faction::MigrateBuildingDeploys_(BaseId_t baseId, Faction& rReceiver)
{
    for (auto it = m_buildingDeploys.begin(); it != m_buildingDeploys.end();)
    {
        if (it->baseId == baseId)
        {
            rReceiver.m_buildingDeploys.push_back(*it);
            it = m_buildingDeploys.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Faction::DropBuildingDeploys_(BaseId_t baseId)
{
    std::erase_if(m_buildingDeploys, [baseId](const BuildingDeploy_t& rDeploy)
    {
        return rDeploy.baseId == baseId;
    });
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
    BaseManager& rAdded = *pBase;
    m_bases.push_back(std::move(pBase));
    m_baseListRevision.Bump();
    if (m_onBaseListChanged)
    {
        m_onBaseListChanged();
    }
    RebuildVisibility();
    // Single "a base now exists in this faction" hook — founding, load, and post-transfer
    // adopt alike. EventBridge wires from this so conquest/probe/diplomacy callers never
    // need to remember WireBase (see docs/architecture/high-level.md, "Object lifetime").
    OnBaseAdded.Emit(rAdded);
}

std::optional<BaseSnapshot_t> Faction::ExtractBase(BaseId_t baseId)
{
    for (auto it = m_bases.begin(); it != m_bases.end(); ++it)
    {
        if ((*it)->GetBaseId() == baseId)
        {
            BaseSnapshot_t snapshot = (*it)->CaptureSnapshot();
            m_bases.erase(it);
            DropBuildingDeploys_(baseId);
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

std::unique_ptr<BaseManager> Faction::ReleaseBase(BaseId_t baseId)
{
    for (auto it = m_bases.begin(); it != m_bases.end(); ++it)
    {
        if ((*it)->GetBaseId() == baseId)
        {
            std::unique_ptr<BaseManager> pReleased = std::move(*it);
            m_bases.erase(it);
            m_baseListRevision.Bump();
            if (m_onBaseListChanged)
            {
                m_onBaseListChanged();
            }
            RebuildVisibility();
            return pReleased;
        }
    }
    throw std::runtime_error("Faction::ReleaseBase: base not found");
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
        *rDataContext.buildingRegistry,
        *rDataContext.socialRatingRegistry,
        *rDataContext.popTypeRegistry,
        *rDataContext.popTypeAvailabilityCalculator,
        *rDataContext.growthConfig,
        *rDataContext.popCompositionCalculator,
        &rSecretProjectAvailability,
        rTileEffects,
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

namespace
{

// Clear the home-base link of every unit homed at rBase that rBase's owner does not own.
// Iterates a copy: SetHomeBase(nullptr) releases the claim, mutating the index's vector.
void DropForeignHomeClaims_(BaseManager& rBase)
{
    const Faction* pOwner = &rBase.GetFaction();
    const std::vector<Unit*> homed = rBase.GetHomeUnits().GetUnits();
    for (Unit* pUnit : homed)
    {
        if (pUnit && &pUnit->GetFaction() != pOwner)
        {
            pUnit->SetHomeBase(nullptr);
        }
    }
}

} // namespace

void Faction::TransferBaseTo(BaseId_t baseId, Faction& rReceiver)
{
    if (&rReceiver == this)
    {
        throw std::invalid_argument("Faction::TransferBaseTo: cannot transfer to self");
    }

    // Identity-preserving move: same BaseManager object, same baseId, same address — not
    // ExtractBase + CreateBaseFromSnapshot. RebindFaction re-points every per-faction
    // dependency (effects provider, research, economy) the base resolves through.
    std::unique_ptr<BaseManager> pBase = ReleaseBase(baseId);
    BaseManager& rBase = *pBase;
    rBase.RebindFaction(rReceiver);
    MigrateBuildingDeploys_(baseId, rReceiver);
    rReceiver.AddBase(std::move(pBase));

    // The base keeps its HomeBaseIndex across the move, so units of the *previous* owner
    // would still be homed here — and a supply crawler homed at a base feeds that base's
    // production (ResourceManager::ComputeWorked_), i.e. the captor would harvest the loser's
    // crawlers. A claim on a base its owner does not own is a foreign home claim, which the
    // transfer protocol treats as invalid; drop exactly those (same rule ReleaseAndAdopt_
    // applies to unit transfer). Units the receiver already owns keep their home.
    DropForeignHomeClaims_(rBase);

    // Psych (and thus drone/talent targets) may differ under the new owner — same
    // recalculation CreateBaseFromSnapshot used to apply after reconstruct.
    rBase.GetPopulation().RecalculateComposition();
    rBase.GetWorkerAssignments().UnassignAll();
    rBase.GetWorkerAssignments().AutoAssignWorkers();
}

namespace
{

Unit* FindUnitById_(Faction& rFaction, UnitId_t unitId)
{
    for (Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return &rUnit;
        }
    }
    return nullptr;
}

// Release + adopt one unit (giver→receiver), clearing the home-base claim and the production
// base: both name bases of the previous owner, and a claim on a base the receiver does not own
// is a foreign home claim, which the transfer protocol treats as invalid (see
// Unit::ClearProducedAtBase and docs/architecture/high-level.md, "Object lifetime").
Unit& ReleaseAndAdopt_(Faction& rGiver, Faction& rReceiver, Unit& rUnit)
{
    std::unique_ptr<Unit> pReleased = rGiver.GetUnitManager().ReleaseUnit(rUnit);
    Unit& rAdopted = rReceiver.GetUnitManager().AdoptUnit(std::move(pReleased));
    rAdopted.SetHomeBase(nullptr);
    rAdopted.ClearProducedAtBase();
    return rAdopted;
}

} // namespace

void Faction::TransferUnitTo(UnitId_t unitId, Faction& rReceiver)
{
    if (&rReceiver == this)
    {
        throw std::invalid_argument("Faction::TransferUnitTo: cannot transfer to self");
    }

    Unit* pFound = FindUnitById_(*this, unitId);
    if (!pFound)
    {
        throw std::runtime_error("Faction::TransferUnitTo: unit not found");
    }

    // A transferred carrier's cargo travels with it (peaceful transfer never strands or
    // sinks passengers — that is DestroyUnit's combat rule only). Snapshot before releasing:
    // ReleaseUnit/AdoptUnit do not touch cargo/carrier links, but the vector itself is a
    // view into pFound's live state that we are about to hand to another faction.
    const std::vector<Unit*> cargo = pFound->GetCargo();

    // A transferred embarked passenger detaches cleanly rather than dying: no throw, and no
    // CanPlaceUnitOnTile re-check, since the unit never leaves the map (see
    // docs/architecture/high-level.md, "Object lifetime").
    if (pFound->IsEmbarked())
    {
        pFound->Disembark();
    }

    ReleaseAndAdopt_(*this, rReceiver, *pFound);

    for (Unit* pPassenger : cargo)
    {
        if (pPassenger && &pPassenger->GetFaction() == this)
        {
            ReleaseAndAdopt_(*this, rReceiver, *pPassenger);
        }
    }
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
        *rDataContext.buildingRegistry,
        *rDataContext.socialRatingRegistry,
        *rDataContext.popTypeRegistry,
        *rDataContext.popTypeAvailabilityCalculator,
        *rDataContext.growthConfig,
        *rDataContext.popCompositionCalculator,
        &rSecretProjectAvailability,
        rTileEffects);

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

void Faction::BindWorldEffects(IWorldEffectsSource& rWorldEffects)
{
    m_pWorldEffects = &rWorldEffects;
    // Force recomposition on next GetActiveEffects.
    m_composedLocalVersion = UINT64_MAX;
    m_composedWorldStamp = UINT64_MAX;
}

Faction::VisibilityRebuildScope::VisibilityRebuildScope(Faction& rFaction)
    : m_pFaction(&rFaction)
{
    ++m_pFaction->m_visibilityDeferralDepth;
}

Faction::VisibilityRebuildScope::VisibilityRebuildScope(VisibilityRebuildScope&& rOther) noexcept
    : m_pFaction(std::exchange(rOther.m_pFaction, nullptr))
{
}

Faction::VisibilityRebuildScope::~VisibilityRebuildScope()
{
    if (!m_pFaction)
    {
        return;
    }
    --m_pFaction->m_visibilityDeferralDepth;
    if (m_pFaction->m_visibilityDeferralDepth == 0 && m_pFaction->m_bVisibilityDirty)
    {
        m_pFaction->m_bVisibilityDirty = false;
        m_pFaction->RebuildVisibility();
    }
}

Faction::VisibilityRebuildScope Faction::DeferVisibilityRebuild()
{
    return VisibilityRebuildScope(*this);
}

void Faction::RebuildVisibility()
{
    if (m_visibilityDeferralDepth > 0)
    {
        m_bVisibilityDirty = true;
        return;
    }

    m_visible.RebuildFromSources(*this, m_rWorldMap, m_explored);
    ApplyVisibilityRules(*this, m_rSettings);
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
    // Both are guaranteed: LoadGameData/ThrowIfIncomplete rejects a context without the
    // calculator, and m_pResearch is a unique_ptr this constructor always fills.
    return m_rDataContext.popTypeAvailabilityCalculator->GetAvailable(m_pResearch->GetDiscoveredTechs());
}

const FactionEffects_t& Faction::GetLocalActiveEffects() const
{
    return m_effectsPool.Get();
}

uint64_t Faction::GetLocalEffectsVersion() const
{
    return m_effectsPool.GetVersion();
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
