#include "game/visibility/VisibilityMutations.h"
#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"

#include "game/buildings/BuildingRegistry.h"
#include "game/production/StockpileRegistry.h"
#include "game/buildings/BuildingUpkeep.h"
#include "game/economy/CommerceCache.h"
#include "game/DifficultyConfig.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/population/GrowthConfigParser.h"
#include "game/base/BaseConquestConfig.h"
#include "game/map/WorldMap.h"
#include "game/map/MapUtils.h"
#include "game/base/BaseRuleset.h"
#include "game/production/ScrapRefundCalculator.h"
#include "game/production/ProductionRules.h"
#include "game/production/ProductionMutations.h"
#include "game/economy/ResourceManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include "game/units/UnitDesign.h"
#include "game/natives/NativeDesign.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/effects/InteractionResolve.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include "game/faction/AIProfile.h"
#include "game/faction/FactionFlavor.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/EconomyMutations.h"
#include "game/units/Military.h"
#include "game/units/IDesign.h"
#include "game/units/UpgradeMutations.h"
#include "game/units/UpgradeRules.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchSelector.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/units/UnitManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/production/ProductionManager.h"
#include "game/population/Pop.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/population/PopTypeAvailabilityCalculator.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/units/Unit.h"
#include "game/combat/MoraleCalculator.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/ActiveEffect.h"
#include "lib/IdAllocator.h"
#include "game/probes/ProbeActionConfig.h"

namespace ac
{

Faction::Faction(FactionId_t factionId, bool bIsPlayerControlled,
                 const FactionConfig_t& rDefinition,
                 const GameDataContext& rDataContext,
                 WorldMap& rWorldMap,
                 const GameSettings& rSettings,
                 IdAllocator& rUnitIds,
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
    , m_pCommerce(std::make_unique<CommerceCache>(*this))
    , m_pMilitary(std::make_unique<Military>())
    , m_pResearch(std::make_unique<ResearchManager>(*rDataContext.techRegistry,
                                                    *rDataContext.techCostCalculator, this))
    , m_pResearchSelector(std::make_unique<ResearchSelector>(*m_pResearch, seed ^ 0x9E3779B9u))
    , m_pSocialEngineering(std::make_unique<SocialEngineeringManager>(
          *rDataContext.socialPolicyRegistry))
    , m_pUnits(std::make_unique<UnitManager>(*this, *rDataContext.moraleCalculator))
    , m_effectsPool(*this, *rDataContext.buildingRegistry, m_baseListRevision,
                    rDataContext.tileYieldRules.effects, *rDataContext.socialRatingRegistry,
                    rDataContext.productionConfig->effects,
                    rDataContext.baseConquestConfig->effects,
                    rDataContext.policeRules,
                    rDataContext.popCompositionConfig->effects,
                    rDataContext.growthConfig->effects,
                    *rDataContext.difficultyConfig,
                    *rDataContext.nativeLifeConfig,
                    rDataContext.ecoDamageConfig->effects,
                    rDataContext.probeActionsConfig->effects,
                    *rDataContext.diplomacyConfig)
    , m_rWorldMap(rWorldMap)
    , m_rSettings(rSettings)
    , m_rUnitIds(rUnitIds)
    , m_composedEffects(*this)
{
    m_pResearchSelector->EnsureResearchTarget();
    // Sized from the map this faction is bound to for life, then a first reading.
    m_explored.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    m_visible.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    m_tileMemory.Reset(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    RebuildVisibility();

    if (rDataContext.nativeUnitRegistry)
    {
        for (const NativeUnitConfig_t& rConfig : rDataContext.nativeUnitRegistry->GetAll())
        {
            auto pDesign = std::make_unique<NativeDesign>(rConfig);
            if (!m_pMilitary->AddDesign(std::move(pDesign)))
            {
                throw std::runtime_error("Faction: failed to register native design '"
                                         + rConfig.id + "'");
            }
        }
    }
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

CommerceCache& Faction::GetCommerce()
{
    return *m_pCommerce;
}

const CommerceCache& Faction::GetCommerce() const
{
    return *m_pCommerce;
}

int Faction::CollectIncome()
{
    int total = 0;
    for (BaseManager& rBase : Bases())
    {
        total += rBase.GetResources().ConsumeEcon();
    }
    m_pEconomy->AddEnergy(total);
    return total;
}

int Faction::CollectResearch()
{
    int total = 0;
    for (BaseManager& rBase : Bases())
    {
        const int labs = rBase.GetResources().ConsumeLabs();
        total += labs;
        m_pResearch->AddResearchPoints(labs);
        DiscoverCurrentResearch();
    }
    return total;
}

int Faction::GetNetIncomePerTurn() const
{
    int total = 0;
    for (const BaseManager& rBase : Bases())
    {
        total += rBase.GetEconProduction();
    }
    return total - GetBuildingUpkeep();
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

BaseManager* Faction::FindBase(BaseId_t baseId)
{
    return const_cast<BaseManager*>(static_cast<const Faction*>(this)->FindBase(baseId));
}

const BaseManager* Faction::FindBase(BaseId_t baseId) const
{
    for (const BaseManager& rBase : Bases())
    {
        if (rBase.GetBaseId() == baseId)
        {
            return &rBase;
        }
    }
    return nullptr;
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
    // This base's records only, dropping its most-cooling copy: a destroyed copy takes a
    // suppression with it, and retiring an expired one instead would under-count ready copies.
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
    for (const BaseManager& rBase : Bases())
    {
        total += rBase.GetLabsProduction();
    }
    return total;
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
    NoteBaseListChanged_();
    // The single "a base now exists in this faction" hook, which EventBridge wires from.
    OnBaseAdded.Emit(rAdded);
}

void Faction::NoteBaseListChanged_()
{
    m_baseListRevision.Bump();
    OnBaseListChanged.Emit();
    RebuildVisibility();
}

std::optional<BaseSnapshot_t> Faction::ExtractBase(BaseId_t baseId)
{
    for (auto it = m_bases.begin(); it != m_bases.end(); ++it)
    {
        if ((*it)->GetBaseId() == baseId)
        {
            // Move out before erase so ~BaseManager cannot observe a null hole in m_bases
            // (tile-effect teardown walks session factions and Bases()).
            BaseSnapshot_t snapshot = (*it)->CaptureSnapshot();
            std::unique_ptr<BaseManager> pExtracted = std::move(*it);
            m_bases.erase(it);
            pExtracted.reset();
            DropBuildingDeploys_(baseId);
            NoteBaseListChanged_();
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
            NoteBaseListChanged_();
            return pReleased;
        }
    }
    throw std::runtime_error("Faction::ReleaseBase: base not found");
}

namespace
{

// A faction holds one Headquarters, so a base never brings one to its new owner
// (config/deviations/README.md, "A given Headquarters goes with the base").
void DropHeadquarters_(BaseManager& rBase)
{
    BuildingManager& rBuildings = rBase.GetBuildingManager();
    std::vector<BuildingId_t> headquarters;
    for (const BuildingConfig_t* pBuilding : rBuildings.GetBuildings())
    {
        if (ResolveFlag(pBuilding->effects, RuleFlagId_t::Headquarters))
        {
            headquarters.push_back(pBuilding->id);
        }
    }
    for (const BuildingId_t& rId : headquarters)
    {
        rBuildings.DestroyBuilding(rId);
    }
}

void RecomposeUnderOwner_(BaseManager& rBase)
{
    rBase.GetPopulation().RecalculateComposition();
    rBase.GetWorkerAssignments().UnassignAll();
    rBase.GetWorkerAssignments().AutoAssignWorkers();
}

// The first item becomes current production; the rest queue behind it in order.
void RestoreProductionQueue_(ProductionManager& rProduction, const BaseEffects_t& rEffects,
                             const std::vector<const IConstructable*>& rItems)
{
    for (std::size_t i = 0; i < rItems.size(); ++i)
    {
        if (i == 0)
        {
            rProduction.SetProduction(rItems[i], rEffects);
            continue;
        }
        const int slot = static_cast<int>(rProduction.GetQueue().size());
        rProduction.Insert(slot, *rItems[i]);
    }
}

} // namespace

std::vector<const IConstructable*> Faction::ResolveProductionItems_(
    const std::vector<std::string>& rItemIds) const
{
    std::vector<const IConstructable*> items;
    for (const std::string& rItemId : rItemIds)
    {
        const IConstructable* pItem = nullptr;
        if (m_rDataContext.buildingRegistry)
        {
            pItem = m_rDataContext.buildingRegistry->Find(rItemId);
        }
        if (!pItem && m_rDataContext.stockpileRegistry)
        {
            pItem = m_rDataContext.stockpileRegistry->Find(rItemId);
        }
        if (!pItem)
        {
            if (const IDesign* pDesign = GetMilitary().GetDesign(rItemId))
            {
                pItem = dynamic_cast<const IConstructable*>(pDesign);
            }
        }
        if (!pItem)
        {
            throw std::runtime_error(
                "Faction::CreateBaseFromSnapshot: unknown production item '" + rItemId + "'");
        }
        items.push_back(pItem);
    }
    return items;
}

BaseManager* Faction::CreateBaseFromSnapshot(
    const BaseSnapshot_t& rSnapshot,
    TileEffectsContext& rTileEffects)
{
    if (!rSnapshot.pTile)
    {
        throw std::invalid_argument("Faction::CreateBaseFromSnapshot: pTile is null");
    }

    auto pBase = std::make_unique<BaseManager>(
        *this, rSnapshot.baseId, rSnapshot.name, *rSnapshot.pTile,
        MakeBaseRuleset(m_rDataContext),
        rTileEffects,
        rSnapshot.populationSize);

    for (const std::string& buildingId : rSnapshot.buildingIds)
    {
        pBase->GetBuildingManager().AddBuilding(buildingId);
    }

    pBase->GetPopulation().SetNutrientStockpile(rSnapshot.nutrientStockpile);
    pBase->GetPopulation().RestoreMoodState(rSnapshot.mood);
    if (rSnapshot.bStolenResearch)
    {
        pBase->SetStolenResearch();
    }
    if (rSnapshot.bPlagueIntroduced)
    {
        pBase->SetPlagueIntroduced();
    }

    RestoreProductionQueue_(pBase->GetProduction(), pBase->GetBaseEffects(),
                            ResolveProductionItems_(rSnapshot.productionItemIds));
    pBase->GetProduction().SetMineralStockpile(rSnapshot.mineralStockpile);

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));

    // After AddBase: psych may differ under the new owner, and composition needs the base in
    // Bases().
    RecomposeUnderOwner_(*pRawBase);

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

// Clear every home-base link at rBase, whoever owns the unit: the base is leaving the game, so
// no claim on it can stay valid. Same copy-then-iterate reason as DropForeignHomeClaims_.
void DropAllHomeClaims_(BaseManager& rBase)
{
    const std::vector<Unit*> homed = rBase.GetHomeUnits().GetUnits();
    for (Unit* pUnit : homed)
    {
        if (pUnit)
        {
            pUnit->SetHomeBase(nullptr);
        }
    }
}

} // namespace

void Faction::RazeBase(BaseManager& rBase)
{
    if (&rBase.GetFaction() != this)
    {
        throw std::invalid_argument("Faction::RazeBase: base " + std::to_string(rBase.GetBaseId())
                                    + " is not owned by this faction");
    }
    if (rBase.IsRazed())
    {
        return;
    }

    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (!pBuilding || !pBuilding->bIsSecretProject)
        {
            continue;
        }
        OnSecretProjectDestroyed.Emit(pBuilding->id);
    }

    DropAllHomeClaims_(rBase);
    rBase.MarkRazed_();
    DropBuildingDeploys_(rBase.GetBaseId());
    NoteBaseListChanged_();
}

void Faction::ReapRazedBases()
{
    std::erase_if(m_bases, [](const std::unique_ptr<BaseManager>& pBase) {
        return pBase && pBase->IsRazed();
    });
}

size_t Faction::GetBaseCount() const
{
    size_t count = 0;
    for (const std::unique_ptr<BaseManager>& pBase : m_bases)
    {
        if (pBase && !pBase->IsRazed())
        {
            ++count;
        }
    }
    return count;
}

void Faction::TransferBaseTo(BaseId_t baseId, Faction& rReceiver)
{
    if (&rReceiver == this)
    {
        throw std::invalid_argument("Faction::TransferBaseTo: cannot transfer to self");
    }

    // The same object and id, rebound to every per-faction dependency.
    std::unique_ptr<BaseManager> pBase = ReleaseBase(baseId);
    BaseManager& rBase = *pBase;
    DropHeadquarters_(rBase);
    rBase.RebindFaction(rReceiver);
    MigrateBuildingDeploys_(baseId, rReceiver);
    rReceiver.AddBase(std::move(pBase));

    // A claim on a base its owner does not own is invalid, so the previous owner's units lose
    // their home here; the receiver's own units keep theirs.
    DropForeignHomeClaims_(rBase);

    // Riot escalation is a grievance against the old owner, so the ladder restarts.
    rBase.GetPopulation().ResetMoodEscalation();

    // Psych, and so drone and talent targets, may differ under the new owner.
    RecomposeUnderOwner_(rBase);
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

// Release and adopt one unit, clearing its home claim, which names a base of the previous
// owner. Production grants stay.
Unit& ReleaseAndAdopt_(Faction& rGiver, Faction& rReceiver, Unit& rUnit)
{
    std::unique_ptr<Unit> pReleased = rGiver.GetUnitManager().ReleaseUnit(rUnit);
    Unit& rAdopted = rReceiver.GetUnitManager().AdoptUnit(std::move(pReleased));
    rAdopted.SetHomeBase(nullptr);
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

    // Cargo travels with its carrier; a copy, since the vector is the carrier's live state.
    const std::vector<Unit*> cargo = pFound->Transport().GetCargo();

    // An embarked passenger detaches; it never leaves the map, so placement is not re-checked.
    if (pFound->Transport().IsEmbarked())
    {
        pFound->Transport().Disembark();
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

std::optional<ScrapPayout_t> Faction::QuoteScrapBuilding(const BaseManager& rBase,
                                                        const BuildingId_t& buildingId) const
{
    if (&rBase.GetFaction() != this)
    {
        throw std::invalid_argument(
            "Faction::QuoteScrapBuilding: base is not owned by this faction");
    }
    return rBase.QuoteScrapBuilding_(buildingId);
}

int Faction::ScrapBuilding(BaseManager& rBase, const BuildingId_t& buildingId)
{
    if (&rBase.GetFaction() != this)
    {
        throw std::invalid_argument(
            "Faction::ScrapBuilding: base is not owned by this faction");
    }
    return rBase.ScrapBuilding_(buildingId);
}

namespace
{

int CopiesAt_(const BaseManager& rBase, const BuildingId_t& buildingId)
{
    const auto& rBuildings = rBase.GetBuildingManager().GetBuildings();
    const auto copies = std::ranges::count_if(rBuildings, [&buildingId](const BuildingConfig_t* p) {
        return p && p->id == buildingId;
    });
    return static_cast<int>(copies);
}

// One base's per-copy quote, folded into the faction total. Bases that refund to different
// destinations leave the total with none.
void AddToScrapQuote_(std::optional<ScrapPayout_t>& rTotal, const ScrapPayout_t& rOne, int copies,
                      const BuildingId_t& buildingId)
{
    if (!rTotal)
    {
        rTotal = rOne;
        rTotal->amount = 0;
    }
    else if (rOne.refundType != rTotal->refundType)
    {
        throw std::logic_error("Faction::QuoteScrapBuildings: '" + buildingId
                               + "' quoted mixed refund types");
    }
    else if (rOne.destBaseId != rTotal->destBaseId)
    {
        rTotal->destBaseId.reset();
    }
    rTotal->amount += rOne.amount * copies;
}

} // namespace

std::optional<ScrapPayout_t> Faction::QuoteScrapBuildings(const BuildingId_t& buildingId) const
{
    std::optional<ScrapPayout_t> total;
    for (const BaseManager& rBase : Bases())
    {
        const std::optional<ScrapPayout_t> one = QuoteScrapBuilding(rBase, buildingId);
        if (!one)
        {
            continue;
        }

        AddToScrapQuote_(total, *one, CopiesAt_(rBase, buildingId), buildingId);
    }
    return total;
}

int Faction::ScrapBuildings(const BuildingId_t& buildingId)
{
    const std::optional<ScrapPayout_t> payout = QuoteScrapBuildings(buildingId);
    if (!payout)
    {
        throw std::runtime_error(
            "Faction::ScrapBuildings: '" + buildingId
            + "' cannot be scrapped (not constructed at any base, or a secret project)");
    }

    int granted = 0;
    for (BaseManager& rBase : Bases())
    {
        while (QuoteScrapBuilding(rBase, buildingId).has_value())
        {
            granted += ScrapBuilding(rBase, buildingId);
        }
    }
    return granted;
}

std::optional<ScrapPayout_t> Faction::QuoteScrapUnit(const Unit& rUnit) const
{
    if (&rUnit.GetFaction() != this)
    {
        throw std::invalid_argument("Faction::QuoteScrapUnit: unit is not owned by this faction");
    }
    if (!m_rDataContext.scrapRefundCalculator)
    {
        throw std::runtime_error(
            "Faction::QuoteScrapUnit: no ScrapRefundCalculator on GameDataContext");
    }

    ScrapOverride_t override;
    if (const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(&rUnit.GetDesign()))
    {
        for (const UnitComponentConfig_t* pComponent : pUnitDesign->GetComponents())
        {
            if (pComponent && pComponent->scrap)
            {
                MergeScrapOverride(*pComponent->scrap, override);
            }
        }
    }

    const ScrapQuote_t quote = m_rDataContext.scrapRefundCalculator->Quote(
        rUnit.GetDesign().GetBaseCost(), ConstructableKind_t::Unit, override,
        CollectLiveUnitEffects(rUnit).effects);
    if (!quote.bAvailable)
    {
        return std::nullopt;
    }
    const std::optional<BaseId_t> destination = ScrapRefundBase(*this, rUnit.GetTile());
    return PlanScrapPayout(quote, destination);
}

int Faction::ScrapUnit(Unit& rUnit)
{
    const std::optional<ScrapPayout_t> payout = QuoteScrapUnit(rUnit);
    if (!payout)
    {
        throw std::runtime_error(
            "Faction::ScrapUnit: this unit cannot be scrapped (no kinds.unit default_scrap)");
    }

    const int granted = CreditScrapRefund(*payout, *this);
    GetUnitManager().DestroyUnit(rUnit);
    return granted;
}

std::optional<int> Faction::QuoteUpgradeUnit(const Unit& rUnit, const UnitDesign& rNew) const
{
    if (&rUnit.GetFaction() != this)
    {
        throw std::invalid_argument("Faction::QuoteUpgradeUnit: unit is not owned by this faction");
    }
    if (!CanOfferUpgrade(rUnit))
    {
        return std::nullopt;
    }
    const UnitDesign* pOld = dynamic_cast<const UnitDesign*>(&rUnit.GetDesign());
    if (!pOld)
    {
        return std::nullopt;
    }
    return QuoteUpgradeEnergy(*this, *pOld, rNew);
}

bool Faction::UpgradeUnit(Unit& rUnit, const UnitDesign& rNew)
{
    if (&rUnit.GetFaction() != this)
    {
        throw std::invalid_argument("Faction::UpgradeUnit: unit is not owned by this faction");
    }
    return TryUpgradeUnit(rUnit, rNew);
}

BaseManager* Faction::GetHeadquarters()
{
    for (BaseManager& rBase : Bases())
    {
        if (rBase.IsHeadquarters())
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
        if (rBase.IsHeadquarters())
        {
            return &rBase;
        }
    }
    return nullptr;
}

BaseManager* Faction::FindNearestBase(const Tile& rFrom)
{
    return const_cast<BaseManager*>(std::as_const(*this).FindNearestBase(rFrom));
}

const BaseManager* Faction::FindNearestBase(const Tile& rFrom) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    const BaseManager* pBest = nullptr;
    int best = std::numeric_limits<int>::max();
    for (const BaseManager& rBase : Bases())
    {
        const int distance = ChebyshevDistance(rFrom, rBase.GetTile(), mapWidth);
        if (distance < best)
        {
            best = distance;
            pBest = &rBase;
        }
    }
    return pBest;
}

BaseManager* Faction::CreateBase(BaseId_t baseId, const std::string& name, Tile* pTile,
                                  TileEffectsContext& rTileEffects,
                                  std::optional<int> initialPopulation)
{
    if (!pTile)
    {
        throw std::invalid_argument("Faction::CreateBase: pTile is null");
    }
    auto pBase = std::make_unique<BaseManager>(
        *this, baseId, name, *pTile,
        MakeBaseRuleset(m_rDataContext),
        rTileEffects,
        initialPopulation);

    pBase->GetWorkerAssignments().UnassignAll();
    pBase->GetWorkerAssignments().AutoAssignWorkers();

    std::cout << "Created base '" << name << "' with population: " << pBase->GetPopulation().GetSize()
              << " (workers: " << pBase->GetPopulation().GetWorkerCount() << ")\n";

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));

    // TODO: a base founded on water gets a free Pressure Dome, which also yields as Recycling
    // Tanks (docs/thinker/terranx-map.md, "Sea bases"); BaseManager::MayOccupyWater stands in.

    return pRawBase;
}

void Faction::ProduceBaseResources()
{
    for (BaseManager& rBase : Bases())
    {
        rBase.ProduceResources();
    }
}

void Faction::ApplyMineralSupport()
{
    for (BaseManager& rBase : Bases())
    {
        ApplyMineralSupportAtBase(rBase);
    }
}

int Faction::GetBuildingUpkeep() const
{
    int total = 0;
    for (const BaseManager& rBase : Bases())
    {
        total += rBase.GetBuildingUpkeep();
    }
    return total;
}

std::vector<BuildingUpkeepLine_t> Faction::GetBuildingUpkeepByType() const
{
    // Faction-wide UI rollup: faction-lane FacilityEnergyUpkeep only (pOriginBase null),
    // so ThisBase-scoped mods appear on the base panel instead.
    std::vector<const BuildingConfig_t*> owned;
    for (const BaseManager& rBase : Bases())
    {
        const std::vector<const BuildingConfig_t*>& rBuildings =
            rBase.GetBuildingManager().GetBuildings();
        owned.insert(owned.end(), rBuildings.begin(), rBuildings.end());
    }
    return TallyBuildingUpkeepByType(owned, GetActiveEffects().effects, nullptr);
}

void Faction::ApplyBuildingUpkeep()
{
    const int cost = GetBuildingUpkeep();
    if (cost > 0)
    {
        m_pEconomy->SpendEnergy(cost);
    }
}

void Faction::ApplyBaseGrowth()
{
    for (BaseManager& rBase : Bases())
    {
        rBase.ApplyGrowth();
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

    EnsureResearchTarget();
    return true;
}

void Faction::EnsureResearchTarget()
{
    m_pResearchSelector->EnsureResearchTarget();
}

UnitId_t Faction::AllocateUnitId()
{
    return m_rUnitIds.Allocate();
}

UnitPositionIndex& Faction::GetUnitPositions()
{
    return m_rWorldMap.GetUnitPositions();
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

FactionTileMemory& Faction::GetTileMemory()
{
    return m_tileMemory;
}

const FactionTileMemory& Faction::GetTileMemory() const
{
    return m_tileMemory;
}

FactionRevealedUnits& Faction::GetRevealedUnits()
{
    return m_revealedUnits;
}

const FactionRevealedUnits& Faction::GetRevealedUnits() const
{
    return m_revealedUnits;
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
    // Anything the deferred rebuild throws terminates, and that is deliberate: swallowing it would
    // leave stale vision with no diagnostic.
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

    m_visible.RebuildFromSources(*this, m_rWorldMap, m_explored, m_tileMemory);
    ApplyVisibilityRules(*this, m_rSettings);
    OnVisibilityRebuilt.Emit(*this);
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
    // Both accessors funnel through here, so the list and version always describe one sweep.
    const IWorldEffectsSource* pWorld = FindCurrentGameState();
    const uint64_t localVersion = GetLocalEffectsVersion();
    const uint64_t worldStamp = pWorld ? pWorld->GetWorldCompositionStamp(*this) : 0;
    if (localVersion == m_composedLocalVersion && worldStamp == m_composedWorldStamp
        && pWorld == m_pComposedWorld)
    {
        return;
    }

    m_composedEffects = GetLocalActiveEffects();
    if (pWorld)
    {
        const std::vector<ActiveEffect_t> extras = pWorld->CollectWorldExtras(*this);
        m_composedEffects.effects.insert(m_composedEffects.effects.end(), extras.begin(),
                                         extras.end());
    }
    // Deliberately not filtered to the unit-facing scopes: a superset can only cost a
    // wasted collection, never a missed override.
    m_composedInteractionMask = InteractionMaskOf(std::span{m_composedEffects.effects});
    m_composedLocalVersion = localVersion;
    m_composedWorldStamp = worldStamp;
    m_pComposedWorld = pWorld;
    // Monotonic and starting at 1, so equality comparisons cannot collide.
    ++m_composedVersion;
}

const FactionEffects_t& Faction::GetActiveEffects() const
{
    EnsureComposedEffects_();
    return m_composedEffects;
}

InteractionGridMask_t Faction::GetInteractionMask() const
{
    EnsureComposedEffects_();
    return m_composedInteractionMask;
}

uint64_t Faction::GetEffectsVersion() const
{
    // Moves with the local pool and with peer or council inputs.
    EnsureComposedEffects_();
    return m_composedVersion;
}

} // namespace ac
