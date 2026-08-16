#include "game/faction/base/BaseManager.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/IConstructable.h"
#include "game/IEffectsProvider.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/faction/ResearchManager.h"
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
#include "game/stockpiles/StockpileConversion.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/PauseOnEventsConfig.h"
#include <algorithm>
#include <cmath>
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

PauseOnEventId_t ClassifyCompletedItem_(const IConstructable& rItem)
{
    switch (rItem.GetConstructableKind())
    {
    case ConstructableKind_t::Building:
    case ConstructableKind_t::SecretProject:
        return PauseOnEventId_t::NewFacilityBuilt;
    case ConstructableKind_t::Unit:
    {
        const auto* pDesign = dynamic_cast<const UnitDesign*>(&rItem);
        if (pDesign && (pDesign->GetStat(StatId_t::Attack) > 0
                        || pDesign->GetFlag(RuleFlagId_t::ForcesPsiCombat)))
        {
            return PauseOnEventId_t::CombatUnitBuilt;
        }
        return PauseOnEventId_t::NonCombatUnitBuilt;
    }
    case ConstructableKind_t::Stockpile:
        throw std::logic_error("ClassifyCompletedItem_: a stockpile cannot complete");
    }
    throw std::logic_error("ClassifyCompletedItem_: unhandled constructable kind");
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
    int initialPopulation)
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
          *m_pWorkerAssignments, rFaction.GetEconomy(), *this, m_rSocialRatings, m_tile,
          m_rTileEffects, m_homeUnits))
    , m_pProduction(std::make_unique<ProductionManager>(
          rProductionConfig,
          [this]() -> const IConstructable* {
              return m_rStockpileRegistry.FindFallback(
                  m_pFaction->GetResearch().GetDiscoveredTechs());
          }))
    , m_name(std::move(name))
{
    // A base provides its own garrison defense bonus, modeled as the "Base" improvement.
    m_rTileEffects.AddImprovementWithEffects(m_tile, std::string(ImprovementIds::k_Base));

    // What a pop is worth right now, so shrinking a base takes the least productive one.
    // Only BaseManager can resolve a worked tile's yield, which is why the rule is injected
    // rather than living in PopulationManager.
    m_pPopulation->SetPopValuator([this](const Pop& rPop) {
        if (rPop.IsSpecialist())
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
        // A base that has already lost its last pop cannot starve further. It is razed by the
        // Population stage at the end of the turn (GameState::RazeBase), not here: razing
        // destroys this BaseManager, and this lambda is one of its own signal handlers.
        if (m_pPopulation->GetSize() == 0)
        {
            return;
        }
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
    m_pPopulation->OnIsRioting.Connect([this]() {
        OnIsRioting.Emit();
    });

    m_pProduction->OnProductionChanged.Connect([this]() {
        // Switching or clearing the queue cancels an unresolved abandon prompt.
        m_bPendingProductionAbandonConfirm = false;
    });

    m_pProduction->OnProductionCompleted.Connect([this](const std::string& itemId) {
        GameState* pGameState = m_pFaction->GetGameState();
        if (!pGameState)
        {
            throw std::runtime_error(
                "BaseManager: Faction has no GameState bound; cannot complete production");
        }

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
            DispatchInstantaneousEffects(*pBuilding, *this, *pGameState);
            OnProductionCompleted.Emit(itemId);
            return;
        }

        if (const UnitDesign* pDesign = m_pFaction->GetMilitary().GetDesign(itemId))
        {
            m_pFaction->GetUnitManager().CreateUnit(
                pGameState->AllocateUnitId(),
                *pDesign,
                pGameState->GetWorldMap().GetUnitPositions(),
                m_tile,
                this,
                this);
            DispatchInstantaneousEffects(*pDesign, *this, *pGameState);
            OnProductionCompleted.Emit(itemId);
            return;
        }

        throw std::runtime_error(
            "BaseManager: completed production item '" + itemId
            + "' is neither a known building nor a unit design of this faction");
    });
}

BaseManager::~BaseManager()
{
    // Emitted first, while every member is still fully alive, so an observer (BaseView's
    // pop-on-destroy) can drop its reference before the object actually goes away — mirrors
    // UnitManager::OnUnitDestroyed's "signal before erase" contract.
    OnDestroyed.Emit();
    m_rTileEffects.RemoveImprovementWithEffects(m_tile, std::string(ImprovementIds::k_Base));
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

bool BaseManager::UserAssignBestAvailableWorker(const Tile* pTile)
{
    return m_pWorkerAssignments->UserAssignBestAvailableWorker(pTile);
}

int BaseManager::GetNutrientProduction() const
{
    return m_pResources->GetNutrientProduction(BuildBaseEffects_());
}

int BaseManager::GetMineralProduction() const
{
    return m_pResources->GetMineralProduction(BuildBaseEffects_());
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
    if (!pItem || pItem->NeverCompletes())
    {
        return std::nullopt;
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
    return m_pResources->GetEnergyProduction(BuildBaseEffects_());
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

int BaseManager::GetDroneModifier() const
{
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(BuildBaseEffects_(), StatId_t::Drones), 0.0)
            .total);
}

int BaseManager::GetTalentModifier() const
{
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(BuildBaseEffects_(), StatId_t::Talents), 0.0)
            .total);
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
    for (const std::unique_ptr<UnitDesign>& pDesign : m_pFaction->GetMilitary().GetDesigns())
    {
        available.push_back(pDesign.get());
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
    if (m_bPendingProductionAbandonConfirm)
    {
        // ConvertMinerals already claimed the bank; wait for Confirm / Defer.
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }

    // Stamp the turn original without adding minerals: ConvertMinerals already moved this
    // turn's leftover bank onto a real item (or converted / wasted it).
    m_pProduction->BankProduction(0);
    return TryCompleteReadyProduction();
}

ProductionApplyResult_t BaseManager::TryCompleteReadyProduction()
{
    if (m_bPendingProductionAbandonConfirm)
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }

    if (!m_pProduction->HasProduction())
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::Idle, {}};
    }

    const BaseEffects_t& rEffects = BuildBaseEffects_();
    const bool bPrototype = IsCurrentProductionPrototype_();
    if (!m_pProduction->IsReadyToComplete(rEffects, bPrototype))
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }

    if (WouldEmptyBaseOnProductionComplete_())
    {
        m_bPendingProductionAbandonConfirm = true;
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }

    const IConstructable& rItem = *m_pProduction->GetCurrentProduction();
    // TODO: a prototype reports PrototypeBuilt instead of CombatUnitBuilt / NonCombatUnitBuilt,
    // so a player who wants combat-unit pauses but not prototype pauses gets no prompt at all
    // for a prototype combat unit. Whether prototype overrides the item classification or the
    // two gates should both be consulted is an unrecorded UI rules decision.
    const PauseOnEventId_t completedEvent =
        bPrototype ? PauseOnEventId_t::PrototypeBuilt : ClassifyCompletedItem_(rItem);
    const std::string completedName = rItem.GetName();
    return ProductionApplyResult_t{ProductionApplyKind_t::Completed,
                                  m_pProduction->CompleteProduction(rEffects, bPrototype),
                                  completedEvent, completedName};
}

bool BaseManager::HasPendingProductionAbandonConfirm() const
{
    return m_bPendingProductionAbandonConfirm;
}

std::string BaseManager::ConfirmProductionAbandon()
{
    if (!m_bPendingProductionAbandonConfirm)
    {
        throw std::runtime_error(
            "BaseManager::ConfirmProductionAbandon: no pending abandon confirmation");
    }
    // Clear before CompleteProduction: ResetProduction_ emits OnProductionChanged which
    // would also clear the flag, but Confirm must own the transition explicitly.
    m_bPendingProductionAbandonConfirm = false;
    return m_pProduction->CompleteProduction(BuildBaseEffects_(),
                                             IsCurrentProductionPrototype_());
}

void BaseManager::DeferProductionAbandon()
{
    if (!m_bPendingProductionAbandonConfirm)
    {
        throw std::runtime_error(
            "BaseManager::DeferProductionAbandon: no pending abandon confirmation");
    }
    m_bPendingProductionAbandonConfirm = false;
    // Excess / invested minerals are lost; the item stays queued for a fresh stockpile.
    m_pProduction->SetMineralStockpile(0);
}

bool BaseManager::WouldEmptyBaseOnProductionComplete_() const
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    if (!pItem)
    {
        return false;
    }

    const int size = m_pPopulation->GetSize();
    if (const BuildingConfig_t* pBuilding = m_rBuildingRegistry.Find(pItem->GetId()))
    {
        return PredictInstantaneousPopulationSize(pBuilding->effects, size) <= 0;
    }
    if (const UnitDesign* pDesign = m_pFaction->GetMilitary().GetDesign(pItem->GetId()))
    {
        return PredictUnitProductionPopulationSize(*pDesign, size) <= 0;
    }
    return false;
}

int BaseManager::GetMineralCost() const
{
    return m_pProduction->GetMineralCost(BuildBaseEffects_(), IsCurrentProductionPrototype_());
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

std::optional<int> BaseManager::QuoteScrapBuilding(const BuildingId_t& buildingId) const
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
        const ScrapQuote_t quote = m_rScrapCalculator.Quote(pBuilding->mineralCost,
                                                            ConstructableKind_t::Building);
        if (!quote.bAvailable || quote.refundType != ScrapRefundType_t::EnergyCredits)
        {
            return std::nullopt;
        }
        return quote.amount;
    }
    return std::nullopt;
}

int BaseManager::ScrapBuilding(const BuildingId_t& buildingId)
{
    const std::optional<int> refund = QuoteScrapBuilding(buildingId);
    if (!refund)
    {
        throw std::runtime_error(
            "BaseManager::ScrapBuilding: '" + buildingId
            + "' cannot be scrapped at this base (not constructed, or a secret project)");
    }

    m_pBuildings->DestroyBuilding(buildingId);
    m_pFaction->NotifyBuildingDestroyed(m_baseId, buildingId);
    if (*refund > 0)
    {
        m_pFaction->GetEconomy().AddEnergy(*refund);
    }
    return *refund;
}

bool BaseManager::IsCurrentProductionPrototype_() const
{
    // Same test ClassifyCompletedItem_ uses. Resolving the design by id instead would scan
    // every design of the faction on a call GetMineralCost makes from render paths, and would
    // mistake a building for a unit if the two ever shared an id.
    const UnitDesign* pDesign =
        dynamic_cast<const UnitDesign*>(m_pProduction->GetCurrentProduction());
    return pDesign && m_pFaction->GetMilitary().IsPrototype(*pDesign);
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

void BaseManager::ApplyMineralSupport()
{
    ApplyMineralSupportAtBase(*this);
}

void BaseManager::ConvertMinerals()
{
    const IConstructable* pItem = m_pProduction->GetCurrentProduction();
    const int minerals = m_pResources->ConsumeMinerals();
    if (pItem && !pItem->NeverCompletes())
    {
        m_pProduction->BankProduction(minerals);
        return;
    }
    if (!pItem || minerals <= 0)
    {
        return;
    }

    ApplyStockpileConversionAtBase(*this, m_rStockpileRegistry.Get(pItem->GetId()), minerals);
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
            const UnitDesign* pDesign = rFaction.GetMilitary().GetDesign(pItem->GetId());
            if (!pDesign || !pDesign->IsAvailable(rTechs))
            {
                m_pProduction->RebindProductionItem(nullptr);
            }
            else
            {
                m_pProduction->RebindProductionItem(pDesign);
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
