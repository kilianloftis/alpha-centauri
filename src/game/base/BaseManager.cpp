#include "game/base/BaseManager.h"

#include "game/base/BaseRules.h"
#include "game/base/BaseEffectsCache.h"
#include "game/base/BaseRuleset.h"
#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/DifficultyConfig.h"
#include "game/production/IConstructable.h"
#include "game/effects/IEffectsProvider.h"
#include "game/units/Military.h"
#include "game/units/UnitManager.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionManager.h"
#include "game/production/ProductionMutations.h"
#include "game/economy/ResourceManager.h"
#include "game/economy/EconomyMutations.h"
#include "game/economy/EconomyRules.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/population/PopulationManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/production/StockpileConfig.h"
#include "game/production/StockpileRegistry.h"
#include "game/map/ImprovementIds.h"
#include "game/terraform/SurfaceOccupancy.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/units/IDesign.h"
#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/PauseOnEventsConfig.h"
#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ac
{

namespace
{

std::vector<const Tile*> ComputeWorkableTiles_(const TileEffectsContext& rTileEffects, const Tile& tile)
{
    std::vector<const Tile*> tiles;
    ForEachWorkableTile(tile, rTileEffects.GetWorldMap(),
        [&tiles](const Tile* pTile)
        {
            tiles.push_back(pTile);
        });
    return tiles;
}

WorkedTileClaim ClaimCenterTile_(TileEffectsContext& rTileEffects, const Tile& tile)
{
    // The base works its own tile, claimed for life; a worker there is displaced and its base
    // reassigns it. Throws only if the tile is another base's.
    return rTileEffects.GetWorldMap().GetWorkedTiles().ClaimDisplacing(tile, /*bUserAssigned*/false);
}

int ResolveFoundingPopulation_(Faction& rFaction, std::optional<int> initialPopulation)
{
    if (initialPopulation.has_value())
    {
        return *initialPopulation;
    }
    // StartingSize is Base-domain; resolve the Additive stack from the faction pool directly
    // (AllOwnerBases baselines need no base subject).
    const double total = ResolveStatModifiersTotal(
        FilterByStatId(rFaction.GetActiveEffects().effects, StatId_t::StartingSize),
        SeedFor(StatId_t::StartingSize), nullptr);
    const int size = FinalizeResolvedStat(total);
    if (size <= 0)
    {
        throw std::runtime_error(
            "BaseManager: resolved StartingSize must be > 0, got " + std::to_string(size));
    }
    return size;
}

} // namespace

BaseManager::BaseManager(
    Faction& rFaction,
    BaseId_t baseId,
    std::string name,
    Tile& tile,
    const BaseRuleset_t& rRules,
    TileEffectsContext& rTileEffects,
    std::optional<int> initialPopulation)
    : m_pFaction(&rFaction)
    , m_baseId(baseId)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_centerTileClaim(ClaimCenterTile_(rTileEffects, tile))
    , m_homeUnits(*this)
    , m_rBuildingRegistry(rRules.rBuildings)
    , m_rStockpileRegistry(rRules.rStockpiles)
    , m_rSocialRatings(rRules.rSocialRatings)
    , m_rHurryCalculator(rRules.rHurry)
    , m_rScrapCalculator(rRules.rScrap)
    , m_rProductionConfig(rRules.rProduction)
    , m_pPopulation(std::make_unique<PopulationManager>(
          rRules.rPopTypes, rRules.rPopTypeAvailability, rRules.rGrowth, rRules.rComposition,
          rRules.rDrones, rFaction.GetResearch(), *this,
          ResolveFoundingPopulation_(rFaction, initialPopulation)))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(
          ComputeWorkableTiles_(rTileEffects, tile), *m_pPopulation, rTileEffects,
          rTileEffects.GetWorldMap().GetWorkedTiles()))
    , m_pBuildings(std::make_unique<BuildingManager>(rRules.rBuildings, rFaction.GetResearch()))
    , m_pResources(std::make_unique<ResourceManager>(rFaction.GetCommerce(), *this))
    , m_pProduction(std::make_unique<ProductionManager>(
          rRules.rProduction,
          [this]() -> const IConstructable* {
              return m_rStockpileRegistry.FindFallback(
                  m_pFaction->GetResearch().GetDiscoveredTechs());
          },
          *this))
    , m_name(std::move(name))
    , m_effects(*this, m_rSocialRatings, rFaction)
    , m_ecology(*this, rRules.rEcoDamage)
    , m_bMayOccupyWater(tile.IsWater())
{
    m_pBuildings->OnBuildingDestroyed.Connect(
        std::bind_front(&BaseManager::OnBuildingDestroyed_, this));
    // A base provides its own garrison defense bonus, modeled as the "Base" improvement.
    m_rTileEffects.AddOccupantWithEffects(m_tile, std::string(ImprovementIds::k_Base));
    WirePopulation_();
    // Newly created pops start unassigned; auto-assign once after construction.
    m_pWorkerAssignments->AutoAssignWorkers();
    m_pProduction->OnProductionCompleted.Connect(
        std::bind_front(&BaseManager::OnItemCompleted_, this));
}

void BaseManager::OnBuildingDestroyed_(const BuildingConfig_t& rBuilding)
{
    m_pFaction->NotifyBuildingDestroyed(m_baseId, rBuilding.id);
    if (rBuilding.bIsSecretProject)
    {
        m_pFaction->OnSecretProjectDestroyed.Emit(rBuilding.id);
    }
}

void BaseManager::WirePopulation_()
{
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
    m_pPopulation->OnPopLost.Connect([this](int newSize) {
        OnPopLost.Emit(newSize);
        // The one place an empty base is decided gone, whatever emptied it. RazeBase only marks, so
        // returning into the caller is safe.
        if (newSize == 0)
        {
            m_pFaction->RazeBase(*this);
        }
    });
    m_pPopulation->OnIsRioting.Connect([this]() {
        OnIsRioting.Emit();
    });
}

void BaseManager::OnItemCompleted_(const std::string& itemId)
{
    if (m_rBuildingRegistry.Find(itemId))
    {
        CompleteBuilding_(itemId);
        return;
    }
    if (const IDesign* pDesign = m_pFaction->GetMilitary().GetDesign(itemId))
    {
        CompleteUnit_(*pDesign, itemId);
        return;
    }
    throw std::runtime_error(
        "BaseManager: completed production item '" + itemId
        + "' is neither a known building nor a unit design of this faction");
}

void BaseManager::CompleteBuilding_(const std::string& itemId)
{
    // A second copy can still reach FinishReady_ in the same pass. Drop it; AddBuilding's
    // throw stays a programmer-error backstop.
    if (!m_pBuildings->CanAddBuilding(itemId))
    {
        std::cerr << "Base " << m_baseId << " completed '" << itemId
                  << "' but can no longer hold it (already built here, or the secret project "
                     "was claimed elsewhere); the item was dropped\n";
        return;
    }
    m_pBuildings->AddBuilding(itemId);
    OnProductionCompleted.Emit(ProductionCompleted_t{*this, itemId, nullptr});
}

void BaseManager::CompleteUnit_(const IDesign& rDesign, const std::string& itemId)
{
    Unit& rUnit = m_pFaction->GetUnitManager().CreateUnit(
        m_pFaction->AllocateUnitId(), rDesign, m_pFaction->GetUnitPositions(), m_tile, this, this);
    OnProductionCompleted.Emit(ProductionCompleted_t{*this, itemId, &rUnit});
}

BaseManager::~BaseManager()
{
    if (m_bRazed)
    {
        // MarkRazed_ already did both, back when the base left the game.
        return;
    }
    // First, while every member is alive, so observers can drop their reference.
    OnDestroyed.Emit();
    m_rTileEffects.RemoveOccupantWithEffects(m_tile, std::string(ImprovementIds::k_Base));
}

bool BaseManager::IsRazed() const
{
    return m_bRazed;
}

void BaseManager::MarkRazed_()
{
    if (m_bRazed)
    {
        throw std::logic_error("BaseManager::MarkRazed_: base " + std::to_string(m_baseId)
                               + " is already razed");
    }
    m_bRazed = true;
    // The square must be free the instant the base dies — another unit may move onto it, or a
    // colony pod may found here, long before ReapRazedBases destroys this object.
    m_rTileEffects.RemoveOccupantWithEffects(m_tile, std::string(ImprovementIds::k_Base));
    OnDestroyed.Emit();
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
    if (const IConstructable* pCurrent = m_pProduction->GetCurrentProduction())
    {
        snapshot.productionItemIds.push_back(pCurrent->GetId());
    }
    for (const IConstructable* pItem : m_pProduction->GetQueue())
    {
        snapshot.productionItemIds.push_back(pItem->GetId());
    }
    snapshot.mineralStockpile = m_pProduction->GetMineralStockpile();
    snapshot.nutrientStockpile = m_pPopulation->GetNutrientStockpile();
    snapshot.mood = m_pPopulation->CaptureMoodState();
    return snapshot;
}

bool BaseManager::MayOccupyWater() const
{
    return m_bMayOccupyWater || m_pBuildings->HasBuilding(k_PressureDomeBuildingId);
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

bool BaseManager::UserAssignBestAvailableWorker(const Tile* pTile)
{
    return m_pWorkerAssignments->UserAssignBestAvailableWorker(pTile);
}

int BaseManager::GetNutrientProduction() const
{
    return m_pResources->GetNutrientProduction(m_effects.Get());
}

int BaseManager::GetMineralProduction() const
{
    return m_pResources->GetMineralProduction(m_effects.Get());
}

int BaseManager::GetMineralSupportCost() const
{
    return MineralSupportCost(*this);
}

std::optional<int> BaseManager::GetTurnsToProductionCompletion() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem || pItem->IsStockpile() || !CanCompleteProduction(*this))
    {
        return std::nullopt;
    }
    if (m_pProduction->IsCompletionBlocked())
    {
        // Funded and ready; it is waiting on an answer, not on minerals, so no turns remain.
        return 0;
    }
    const int remaining = std::max(0, GetMineralCost() - m_pProduction->GetMineralStockpile());
    if (remaining == 0)
    {
        return 1;
    }
    const int rate = MineralsForProduction(*this);
    if (rate <= 0)
    {
        return std::nullopt;
    }
    return (remaining + rate - 1) / rate;
}

int BaseManager::GetEnergyProduction() const
{
    return m_pResources->GetEnergyProduction(m_effects.Get());
}

int BaseManager::GetEconProduction() const
{
    return m_pResources->GetEconProduction(m_effects.Get());
}

int BaseManager::GetLabsProduction() const
{
    return m_pResources->GetLabsProduction(m_effects.Get());
}

int BaseManager::GetPsychProduction() const
{
    return m_pResources->GetPsychProduction(m_effects.Get());
}

int BaseManager::GetDroneModifier() const
{
    return FinalizeResolvedStat(
        ResolveBaseStat(m_effects.Get(), StatId_t::Drones, SeedFor(StatId_t::Drones), nullptr));
}

int BaseManager::GetTalentModifier() const
{
    return FinalizeResolvedStat(ResolveBaseStat(m_effects.Get(), StatId_t::Talents, 0.0, nullptr));
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

std::vector<const BuildingConfig_t*> BaseManager::GetGrantedBuildings() const
{
    std::vector<const BuildingConfig_t*> granted;
    std::unordered_set<BuildingId_t> seen;
    for (const ActiveEffect_t& rEffect : GetBaseEffects().effects)
    {
        const GrantBuildingEffect_t* pGrant =
            std::get_if<GrantBuildingEffect_t>(&rEffect.config->effect);
        if (!pGrant)
        {
            continue;
        }
        if (!seen.insert(pGrant->buildingId).second)
        {
            continue;
        }
        granted.push_back(&m_rBuildingRegistry.Get(pGrant->buildingId));
    }
    return granted;
}

ProductionManager& BaseManager::GetProduction()
{
    return *m_pProduction;
}

const ProductionManager& BaseManager::GetProduction() const
{
    return *m_pProduction;
}

ProductionApplyResult_t BaseManager::ApplyProduction()
{
    // Riot: do not ConsumeMinerals / BankProduction. Mid-turn TryCompleteReady still sees
    // productionDisabled via the completion probe — both gates are required.
    if (!CanBankProduction(*this))
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }
    m_pProduction->BankProduction(m_pResources->ConsumeMinerals());
    return m_pProduction->ApplyProduction();
}

ProductionApplyResult_t BaseManager::TryCompleteReadyProduction()
{
    return m_pProduction->TryCompleteReady();
}

bool BaseManager::IsProductionDisabled() const
{
    return ResolveFlag(*this, RuleFlagId_t::DisableProduction);
}

bool BaseManager::HasPendingProductionConfirmation() const
{
    return m_pProduction->HasPendingConfirmation();
}

std::string BaseManager::CompletePendingProduction()
{
    return m_pProduction->CompletePending();
}

void BaseManager::DeferProductionCompletion()
{
    m_pProduction->DeferCompletion();
}

int BaseManager::GetMineralCost() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem)
    {
        return 0;
    }
    return ItemMineralCost(*this, *pItem);
}

HurryInputs_t BaseManager::BuildHurryInputs_(const IConstructable& rItem) const
{
    const int stockpile = m_pProduction->GetMineralStockpile();
    return HurryInputs_t{std::max(0, GetMineralCost() - stockpile), stockpile,
                         rItem.GetConstructableKind()};
}

HurryQuote_t BaseManager::QuoteHurry() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem)
    {
        return {};
    }
    return m_rHurryCalculator.Quote(BuildHurryInputs_(*pItem));
}

HurryResult_t BaseManager::HurryProduction(int energyCredits)
{
    if (energyCredits <= 0)
    {
        throw std::invalid_argument("BaseManager::HurryProduction: energyCredits "
                                    + std::to_string(energyCredits) + " must be positive");
    }

    if (!CanHurry(*this))
    {
        throw std::runtime_error("BaseManager::HurryProduction: production is disabled at this "
                                 "base");
    }

    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem)
    {
        throw std::runtime_error("BaseManager::HurryProduction: nothing is queued, so there is "
                                 "nothing that can be hurried");
    }

    // ApplyCredits throws for a kind with no formula — a stockpile, or one a mod switched off.
    const HurrySpend_t spend =
        m_rHurryCalculator.ApplyCredits(BuildHurryInputs_(*pItem), energyCredits);
    HurryResult_t result;
    result.production = ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    if (spend.creditsSpent <= 0)
    {
        return result;
    }

    // SpendEnergy owns the overdraft rule and throws before anything is granted.
    m_pFaction->GetEconomy().SpendEnergy(spend.creditsSpent);
    m_pProduction->SetMineralStockpile(m_pProduction->GetMineralStockpile()
                                       + spend.mineralsAdded);
    result.creditsSpent = spend.creditsSpent;
    result.mineralsAdded = spend.mineralsAdded;
    result.production = TryCompleteReadyProduction();
    return result;
}

std::optional<ScrapPayout_t> BaseManager::QuoteScrapBuilding_(const BuildingId_t& buildingId) const
{
    for (const BuildingConfig_t* pBuilding : m_pBuildings->GetBuildings())
    {
        if (!pBuilding || pBuilding->id != buildingId)
        {
            continue;
        }
        if (!CanScrapBuilding(*pBuilding))
        {
            return std::nullopt;
        }
        const ScrapQuote_t quote =
            m_rScrapCalculator.Quote(pBuilding->mineralCost, ConstructableKind_t::Building,
                                     pBuilding->scrap.value_or(ScrapOverride_t{}),
                                     m_effects.Get().effects);
        if (!quote.bAvailable)
        {
            return std::nullopt;
        }
        return PlanScrapPayout(quote, m_baseId);
    }
    return std::nullopt;
}

int BaseManager::ScrapBuilding_(const BuildingId_t& buildingId)
{
    const std::optional<ScrapPayout_t> payout = QuoteScrapBuilding_(buildingId);
    if (!payout)
    {
        throw std::runtime_error(
            "Faction::ScrapBuilding: '" + buildingId
            + "' cannot be scrapped at this base (not constructed, or a secret project)");
    }

    m_pBuildings->DestroyBuilding(buildingId);
    return CreditScrapRefund(*payout, *m_pFaction);
}

int BaseManager::GetEffectiveSocialRating(SocialRatingId_t rating) const
{
    return m_effects.GetEffectiveRating(rating);
}

void BaseManager::ProduceResources()
{
    m_pResources->ProduceResources(m_effects.Get());
}

std::vector<BuildingUpkeepLine_t> BaseManager::GetBuildingUpkeepByType() const
{
    return TallyBuildingUpkeepByType(m_pBuildings->GetBuildings(),
                                     GetFaction().GetActiveEffects().effects, this);
}

int BaseManager::GetBuildingUpkeep() const
{
    return SumBuildingUpkeep(GetBuildingUpkeepByType());
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
    m_pPopulation->ApplyGrowth(m_pResources->ConsumeNutrients(), m_effects.Get());
}

int BaseManager::GetNutrientsRequired() const
{
    return m_pPopulation->GetNutrientsRequired(m_effects.Get());
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
    return m_effects.Get();
}

int BaseManager::GetEcologicalDamage() const
{
    return m_ecology.GetDamage();
}

TileYieldView_t BaseManager::GetWorkedTileYield(const Tile& rTile) const
{
    return m_pWorkerAssignments->GetWorkedTileYield(rTile, m_effects.Get());
}

TileYieldView_t BaseManager::GetPreviewTileYield(const Tile& rTile) const
{
    return m_rTileEffects.ResolveTileYield(rTile, /*bIsBaseTile*/ false, m_effects.Get());
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
    m_effects.BindProvider(rFaction);
    m_pPopulation->RebindResearch(rFaction.GetResearch());
    m_pBuildings->RebindResearch(rFaction.GetResearch());
    m_pResources->RebindCommerce(rFaction.GetCommerce());

    RehomeProduction_(rFaction);
}

void BaseManager::RehomeProduction_(const Faction& rFaction)
{
    const IConstructable* pReboundCurrent =
        RehomeItem_(m_pProduction->GetCurrentProduction(), rFaction);
    std::vector<const IConstructable*> reboundQueue;
    for (const IConstructable* pItem : m_pProduction->GetQueue())
    {
        if (const IConstructable* pRebound = RehomeItem_(pItem, rFaction))
        {
            reboundQueue.push_back(pRebound);
        }
    }
    m_pProduction->RebindCurrent(pReboundCurrent);
    m_pProduction->RebindQueue(std::move(reboundQueue));
}

// Buildings and stockpiles are shared configs and stay if the new owner has the tech. A unit
// design re-homes to the new owner's copy if it can build it, else the slot drops.
const IConstructable* BaseManager::RehomeItem_(const IConstructable* pItem,
                                               const Faction& rFaction) const
{
    if (!pItem)
    {
        return nullptr;
    }
    const std::vector<std::string>& rTechs = rFaction.GetResearch().GetDiscoveredTechs();
    const std::string& rId = pItem->GetId();
    if (const BuildingConfig_t* pBuilding = m_rBuildingRegistry.Find(rId))
    {
        return pBuilding->IsAvailable(rTechs) ? pBuilding : nullptr;
    }
    if (const StockpileConfig_t* pStockpile = m_rStockpileRegistry.Find(rId))
    {
        return pStockpile->IsAvailable(rTechs) ? pStockpile : nullptr;
    }
    const IDesign* pDesign = rFaction.GetMilitary().GetDesign(rId);
    const auto* pConstructable = dynamic_cast<const IConstructable*>(pDesign);
    const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign);
    if (pConstructable && (!pUnitDesign || pUnitDesign->IsAvailable(rTechs)))
    {
        return pConstructable;
    }
    return nullptr;
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
