#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BaseEffectsCache.h"
#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/DifficultyConfig.h"
#include "game/IConstructable.h"
#include "game/IEffectsProvider.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/MineralSupport.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/stockpiles/StockpileConfig.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/map/ImprovementIds.h"
#include "game/map/SurfaceOccupancy.h"
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

int ResolveFoundingPopulation_(Faction& rFaction, std::optional<int> initialPopulation)
{
    if (initialPopulation.has_value())
    {
        return *initialPopulation;
    }
    // StartingSize is Base-domain; resolve the Additive stack from the faction pool directly
    // (AllOwnerBases baselines need no base subject).
    const int size = FinalizeResolvedStat(
        ResolveStatModifiersTotal(
            FilterByStatId(rFaction.GetActiveEffects().effects, StatId_t::StartingSize),
            SeedFor(StatId_t::StartingSize)));
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
    const BuildingRegistry& rBuildingRegistry,
    const StockpileRegistry& rStockpileRegistry,
    const SocialRatingRegistry& rSocialRatingRegistry,
    const PopTypeRegistry& rPopTypeRegistry,
    const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
    const GrowthConfig_t& rGrowthConfig,
    const ProductionConfig_t& rProductionConfig,
    const HurryProductionCalculator& rHurryCalculator,
    const ScrapRefundCalculator& rScrapCalculator,
    PopCompositionCalculator& rCompositionCalculator,
    const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
    TileEffectsContext& rTileEffects,
    std::optional<int> initialPopulation,
    bool bMayOccupyWater)
    : m_pFaction(&rFaction)
    , m_baseId(baseId)
    , m_tile(tile)
    , m_rTileEffects(rTileEffects)
    , m_centerTileClaim(ClaimCenterTile_(rTileEffects, tile))
    , m_homeUnits(*this)
    , m_rBuildingRegistry(rBuildingRegistry)
    , m_rStockpileRegistry(rStockpileRegistry)
    , m_rSocialRatings(rSocialRatingRegistry)
    , m_rHurryCalculator(rHurryCalculator)
    , m_rScrapCalculator(rScrapCalculator)
    , m_pPopulation(std::make_unique<PopulationManager>(
          rPopTypeRegistry, rPopTypeAvailabilityCalculator, rGrowthConfig,
          rCompositionCalculator, rFaction.GetResearch(), *this,
          ResolveFoundingPopulation_(rFaction, initialPopulation)))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(
          ComputeWorkableTiles_(rTileEffects, tile), *m_pPopulation, rTileEffects,
          rTileEffects.GetWorldMap().GetWorkedTiles()))
    , m_pBuildings(std::make_unique<BuildingManager>(rBuildingRegistry, pSecretProjectCalculator,
                                                     rFaction.GetResearch()))
    , m_pResources(std::make_unique<ResourceManager>(
          *m_pWorkerAssignments, rFaction.GetEconomy(), rFaction.GetCommerce(), *this,
          m_rSocialRatings, m_tile, m_rTileEffects, m_homeUnits))
    , m_pProduction(std::make_unique<ProductionManager>(
          rProductionConfig,
          [this]() -> const IConstructable* {
              return m_rStockpileRegistry.FindFallback(
                  m_pFaction->GetResearch().GetDiscoveredTechs());
          },
          *this))
    , m_name(std::move(name))
    , m_effects(*this, m_rSocialRatings, rFaction)
    , m_bMayOccupyWater(bMayOccupyWater)
{
    m_pBuildings->OnBuildingDestroyed.Connect([this](const BuildingConfig_t& rBuilding)
    {
        m_pFaction->NotifyBuildingDestroyed(m_baseId, rBuilding.id);
        if (rBuilding.bIsSecretProject)
        {
            m_pFaction->OnSecretProjectDestroyed.Emit(rBuilding.id);
        }
    });

    // A base provides its own garrison defense bonus, modeled as the "Base" improvement.
    m_rTileEffects.AddImprovementWithEffects(m_tile, std::string(ImprovementIds::k_Base));

    // What a pop is worth right now, so shrinking a base takes the least productive one.
    // Only BaseManager can resolve a worked tile's yield, which is why the rule is injected
    // rather than living in PopulationManager.
    m_pPopulation->SetPopValuator([this](const Pop& rPop) {
        if (!rPop.IsWorker())
        {
            const SpecialistOutput_t output = rPop.GetSpecialistOutput();
            return output.econ + output.labs + output.psych;
        }
        const Tile* pTile = rPop.GetTile();
        if (!pTile)
        {
            // An unassigned worker produces nothing, so it is the first to go.
            return 0;
        }
        const TileResources_t yield = GetWorkedTileYield(*pTile).effective;
        return yield.nutrients + yield.minerals + yield.energy;
    });

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
        // The one place "a base with no pops is gone" is decided, whatever emptied it —
        // starvation, a production pop cost, genetic plague, conquest, a world event.
        // RazeBase only marks; this object stays alive until the next ReapRazedBases, so
        // returning into the middle of whatever caused the loss is safe.
        if (newSize == 0)
        {
            m_pFaction->RazeBase(*this);
        }
    });
    m_pPopulation->OnIsRioting.Connect([this]() {
        OnIsRioting.Emit();
    });

    m_pProduction->OnProductionCompleted.Connect([this](const std::string& itemId) {
        if (const BuildingConfig_t* pBuilding = m_rBuildingRegistry.Find(itemId))
        {
            // Losing a race for a secret project, or finishing a copy of something already here, is
            // an ordinary game outcome — the player can queue the same project in two bases, and
            // both are offered it. Report and drop the item; AddBuilding's throw stays a
            // programmer-error backstop.
            // TODO: SMAC's rule for a pre-empted build (refund the minerals? auto-switch?) is not
            // recorded here, so the stockpile is simply left as ProductionManager set it.
            if (!m_pBuildings->CanAddBuilding(itemId))
            {
                std::cerr << "Base " << m_baseId << " completed '" << itemId
                          << "' but can no longer hold it (already built here, or the secret project "
                             "was claimed elsewhere); the item was dropped\n";
                return;
            }
            m_pBuildings->AddBuilding(itemId);
            // Pending nutrient growth commits before on_complete_effects so a pop-cost item
            // (and Hab raising the cap) sees the post-growth size. Hab added above unlocks
            // CanGrow for a full tank at the old max. Intake for the new citizen is applied
            // later in ApplyGrowth (gross bank − post-growth size × intake).
            if (m_pPopulation->CommitPendingGrowth(m_pResources->GetNutrientBank(),
                                                   m_effects.Get()))
            {
                m_pPopulation->EnsureCompositionCurrent();
            }
            OnProductionCompleted.Emit(ProductionCompleted_t{*this, itemId, nullptr});
            return;
        }

        if (const IDesign* pDesign = m_pFaction->GetMilitary().GetDesign(itemId))
        {
            Unit& rUnit = m_pFaction->GetUnitManager().CreateUnit(
                m_pFaction->AllocateUnitId(),
                *pDesign,
                m_pFaction->GetUnitPositions(),
                m_tile,
                this,
                this);
            if (m_pPopulation->CommitPendingGrowth(m_pResources->GetNutrientBank(),
                                                   m_effects.Get()))
            {
                m_pPopulation->EnsureCompositionCurrent();
            }
            OnProductionCompleted.Emit(ProductionCompleted_t{*this, itemId, &rUnit});
            return;
        }

        throw std::runtime_error(
            "BaseManager: completed production item '" + itemId
            + "' is neither a known building nor a unit design of this faction");
    });
}

BaseManager::~BaseManager()
{
    if (m_bRazed)
    {
        // MarkRazed_ already did both, back when the base left the game.
        return;
    }
    // Emitted first, while every member is still fully alive, so an observer (BaseView's
    // pop-on-destroy) can drop its reference before the object actually goes away — mirrors
    // UnitManager::OnUnitDestroyed's "signal before erase" contract.
    OnDestroyed.Emit();
    m_rTileEffects.RemoveImprovementWithEffects(m_tile, std::string(ImprovementIds::k_Base));
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
    m_rTileEffects.RemoveImprovementWithEffects(m_tile, std::string(ImprovementIds::k_Base));
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
    if (const IConstructable* pProduction = m_pProduction->GetCurrentProduction())
    {
        snapshot.productionItemId = pProduction->GetId();
    }
    snapshot.mineralStockpile = m_pProduction->GetMineralStockpile();
    snapshot.nutrientStockpile = m_pPopulation->GetNutrientStockpile();
    snapshot.mood = m_pPopulation->CaptureMoodState();
    snapshot.bMayOccupyWater = m_bMayOccupyWater;
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

int BaseManager::GetMineralsForProduction() const
{
    return std::max(0, GetMineralProduction() - GetMineralSupportCost());
}

std::optional<int> BaseManager::GetTurnsToProductionCompletion() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem || pItem->IsStockpile() || IsProductionDisabled())
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
    const int rate = GetMineralsForProduction();
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
        ResolveBaseStat(m_effects.Get(), StatId_t::Drones, SeedFor(StatId_t::Drones)));
}

int BaseManager::GetTalentModifier() const
{
    return FinalizeResolvedStat(ResolveBaseStat(m_effects.Get(), StatId_t::Talents, 0.0));
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

std::vector<const IConstructable*> BaseManager::GetConstructable() const
{
    std::vector<const IConstructable*> available;
    for (const BuildingConfig_t* pBuilding : m_pBuildings->GetBuildingsAvailableForConstruction())
    {
        available.push_back(pBuilding);
    }
    const std::vector<std::string>& rDiscoveredTechs =
        m_pFaction->GetResearch().GetDiscoveredTechs();
    for (const StockpileConfig_t& rStockpile : m_rStockpileRegistry.GetAll())
    {
        if (rStockpile.IsAvailable(rDiscoveredTechs))
        {
            available.push_back(&rStockpile);
        }
    }
    for (const std::unique_ptr<IDesign>& pDesign : m_pFaction->GetMilitary().GetDesigns())
    {
        if (const auto* pItem = dynamic_cast<const IConstructable*>(pDesign.get()))
        {
            available.push_back(pItem);
        }
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

ProductionApplyResult_t BaseManager::ApplyProduction()
{
    // Riot: do not ConsumeMinerals / BankProduction. Mid-turn TryCompleteReady still sees
    // productionDisabled via the completion probe — both gates are required.
    if (IsProductionDisabled())
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }
    m_pProduction->BankProduction(m_pResources->ConsumeMinerals());
    return m_pProduction->ApplyProduction();
}

ProductionApplyResult_t BaseManager::TryCompleteReadyProduction()
{
    return m_pProduction->TryCompleteReady(/*bNewTurn=*/false);
}

bool BaseManager::IsProductionDisabled() const
{
    return ResolveFlag(*this, RuleFlagId_t::DisableProduction);
}

bool BaseManager::WouldCompletionAbandonBase() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem)
    {
        return false;
    }

    // Judge the pop cost against the size the base ends the turn at. BaseGrowth runs after
    // BaseProduction, so both pending pop events have to be predicted here or the answer is
    // one stage stale — and a base that starves after paying a pop cost is razed without ever
    // having been asked. The two gates are mutually exclusive.
    int size = m_pPopulation->GetSize();
    if (m_pPopulation->WouldGrowThisTurn(m_pResources->GetNutrientBank(), m_effects.Get()))
    {
        ++size;
    }
    else if (m_pPopulation->WouldStarveThisTurn(m_pResources->GetNutrientBank()))
    {
        // Floored at 1: the question is whether *this item* empties the base. A base already
        // starving out from size 1 is lost whatever it builds, and predicting size 0 here would
        // make every item — pop cost or not — answer yes.
        size = std::max(1, size - 1);
    }
    if (const BuildingConfig_t* pBuilding = m_rBuildingRegistry.Find(pItem->GetId()))
    {
        return PredictTriggeredPopulationSize(pBuilding->onCompleteEffects, size) <= 0;
    }
    if (const auto* pDesign =
        dynamic_cast<const UnitDesign*>(m_pFaction->GetMilitary().GetDesign(pItem->GetId())))
    {
        return PredictUnitProductionPopulationSize(*pDesign, size) <= 0;
    }
    return false;
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
    return m_pProduction->GetMineralCost(m_effects.Get(), m_pProduction->IsCurrentPrototype());
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

    if (IsProductionDisabled())
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
        if (pBuilding->bIsSecretProject)
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

void BaseManager::ApplyMineralSupport()
{
    ApplyMineralSupportAtBase(*this);
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
    m_pResources->RebindEconomy(rFaction.GetEconomy());
    m_pResources->RebindCommerce(rFaction.GetCommerce());

    // Buildings live in the shared registry, so a queued building pointer stays valid when
    // the new owner has its required tech. Unit designs are owned by Military: re-home to the
    // new owner's copy when they have the design and the component techs, otherwise clear
    // (mineral stockpile kept either way).
    if (const IConstructable* pItem = m_pProduction->GetCurrentProduction())
    {
        const std::vector<std::string>& rTechs = rFaction.GetResearch().GetDiscoveredTechs();
        if (const BuildingConfig_t* pBuilding = m_rBuildingRegistry.Find(pItem->GetId()))
        {
            if (!pBuilding->IsAvailable(rTechs))
            {
                m_pProduction->RebindProductionItem(nullptr);
            }
        }
        else
        {
            const IDesign* pDesign = rFaction.GetMilitary().GetDesign(pItem->GetId());
            const auto* pConstructable = dynamic_cast<const IConstructable*>(pDesign);
            const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign);
            if (!pConstructable || (pUnitDesign && !pUnitDesign->IsAvailable(rTechs)))
            {
                m_pProduction->RebindProductionItem(nullptr);
            }
            else
            {
                m_pProduction->RebindProductionItem(pConstructable);
            }
        }
    }
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
