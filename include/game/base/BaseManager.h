#pragma once

#include <set>

#include "game/production/IConstructable.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingUpkeep.h"
#include "game/base/BaseTypes.h"
#include "game/production/ProductionApplyResult.h"
#include "game/production/ProductionConfigParser.h"
#include "game/production/HurryProductionCalculator.h"
#include "game/production/ScrapRefundCalculator.h"
#include "game/production/ProductionRules.h"
#include "game/base/BaseEffectsCache.h"
#include "game/base/HomeBaseIndex.h"
#include "game/ecology/BaseEcology.h"
#include "game/map/WorkedTileIndex.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "lib/Signal.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Pop;
class PopulationManager;
class WorkerAssignmentManager;
class EconomyManager;
class ResourceManager;
class BuildingManager;
class BuildingRegistry;
class StockpileRegistry;
class ProductionManager;
class ResearchManager;
class SocialRatingRegistry;
class IEffectsProvider;
class Tile;
class Faction;
struct BaseRuleset_t;
class Unit;

// Outcome of BaseManager::HurryProduction: what the treasury paid for, and what the resulting
// stockpile did to the queued item.
struct HurryResult_t
{
    int creditsSpent = 0;
    int mineralsAdded = 0;
    ProductionApplyResult_t production;
};

// Non-recomputable state needed to destroy a base and rebuild it under a new owner.
// Worked tiles and pop roles are omitted — recalculated after reconstruct (psych changes).
struct BaseSnapshot_t
{
    BaseId_t baseId = 0;
    std::string name;
    Tile* pTile = nullptr;
    int populationSize = 0;
    std::vector<std::string> buildingIds;
    // Current production first, then the upcoming queue. Empty when nothing is current
    // and no fallback stockpile is configured.
    std::vector<std::string> productionItemIds;
    int mineralStockpile = 0;  // progress toward current production
    int nutrientStockpile = 0; // growth bank
    // Mood state (riot / golden age).
    MoodState_t mood;
    bool bStolenResearch = false;
    bool bPlagueIntroduced = false;
};

// Outcome published after BaseManager mutates inventory / spawns a unit for a completed
// production item. Session subscribers (on_complete effects) hang off this signal.
struct ProductionCompleted_t
{
    BaseManager& rBase;
    const std::string& itemId;
    Unit* pUnit = nullptr; // null for buildings
};

// Coordinates a base's subsystems. A method lives here only when it spans two or more.
class BaseManager
{
public:
    // rFaction must outlive the base. rRules names the ruleset pieces a base reads. Research,
    // economy and effects come from the owning faction.
    BaseManager(
        Faction& rFaction,
        BaseId_t baseId,
        std::string name,
        Tile& tile,
        const BaseRuleset_t& rRules,
        TileEffectsContext& rTileEffects,
        std::optional<int> initialPopulation);
    ~BaseManager();

    // Capture identity, size, buildings, production, and stockpiles for transfer.
    BaseSnapshot_t CaptureSnapshot() const;

    // Population subsystem. Pure population queries and mutations go through this reference;
    // BaseManager only keeps operations that coordinate population with other subsystems.
    PopulationManager& GetPopulation();
    const PopulationManager& GetPopulation() const;

    // Convert a pop while keeping worker-tile assignments consistent: unassigns the pop's
    // tile before converting and re-runs auto-assignment if it is a worker afterwards.
    void ConvertPop(Pop& rPop, const std::string& typeId);

    // Signals forwarded from PopulationManager
    Signal<int> OnPopGained;
    Signal<int> OnPopLost;
    Signal<> OnIsRioting;

    // Signals forwarded from ProductionManager (enriched with spawn outcome).
    Signal<const ProductionCompleted_t&> OnProductionCompleted;

    // Worker assignment - delegated to WorkerAssignmentManager
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // User-assigns the best available pop to pTile: unassigned, then a specialist, then another
    // worker. False when nothing changed.
    bool UserAssignBestAvailableWorker(const Tile* pTile);

    // Resource production per turn (calculated live).
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    // Turns until the queued item completes; empty when it never will. 1 if already met.
    std::optional<int> GetTurnsToProductionCompletion() const;
    // Pre-inefficiency energy from worked tiles + flat Energy mods (Economy SE, etc.).
    // Does not include commerce.
    int GetEnergyProduction() const;
    // Post-split production; commerce is resolved inside ResourceManager via CommerceCache.
    int GetEconProduction() const;
    int GetLabsProduction() const;
    // Local energy-psych share + this base's Psych StatModifiers.
    int GetPsychProduction() const;

    // Resource subsystem: per-turn stockpiles and their consumption.
    ResourceManager& GetResources();
    const ResourceManager& GetResources() const;

    // Building subsystem.
    BuildingManager& GetBuildingManager();
    const BuildingManager& GetBuildingManager() const;

    // True when this base may sit on water: it was built on water, or it has Pressure_Dome.
    // Checked when the tile becomes water.
    bool MayOccupyWater() const;

    // SMAC BSTATE_RESEARCH_DATA_STOLEN: a successful steal-tech at this base.
    bool HasStolenResearch() const { return m_bStolenResearch; }
    void SetStolenResearch() { m_bStolenResearch = true; }

    // SMAC BSTATE_GENETIC_PLAGUE: a genetic plague has already hit this base.
    bool HasPlagueIntroduced() const { return m_bPlagueIntroduced; }
    void SetPlagueIntroduced() { m_bPlagueIntroduced = true; }

    // This base's building effects, with ThisBase-scoped ones stamped with this base's identity.
    std::vector<ActiveEffect_t> CollectBuildingEffects() const;

    // Buildings granted by continuous GrantBuilding, by id, including ones also built here.
    std::vector<const BuildingConfig_t*> GetGrantedBuildings() const;


    // Production subsystem.
    ProductionManager& GetProduction();
    const ProductionManager& GetProduction() const;

    // Banks this turn's leftover minerals, converting if a stockpile is queued. Does not complete;
    // under DisableProduction it consumes nothing.
    ProductionApplyResult_t ApplyProduction();

    // Completes the queued item if its stockpile already meets cost, without this turn's bank
    // (after a prototype surcharge drop, or a hurry).
    ProductionApplyResult_t TryCompleteReadyProduction();

    // DisableProduction is in force, which means riot. A deferred completion is something else
    // (ProductionManager::IsCompletionBlocked).
    bool IsProductionDisabled() const;


    // True when ApplyProduction returned AwaitingConfirmation and the player has not yet chosen
    // CompletePendingProduction or DeferProductionCompletion.
    bool HasPendingProductionConfirmation() const;

    // Complete the pending item (an on-complete pop cost may take the base to size 0, which
    // razes it). Throws if nothing is pending. Returns the completed item id.
    std::string CompletePendingProduction();

    // Answer "not this turn": the item stays queued and funded, and is asked about again next
    // turn against the base's size then. Throws if nothing is pending.
    void DeferProductionCompletion();

    // Stockpile configs for conversion and the empty-queue fallback.
    const StockpileRegistry& GetStockpileRegistry() const { return m_rStockpileRegistry; }
    const SocialRatingRegistry& GetSocialRatings() const { return m_rSocialRatings; }

    // The queued item's mineral cost with CostMultiplier and any prototype surcharge; 0 for none or
    // a stockpile.
    int GetMineralCost() const;

    // bAvailable is false when nothing is queued or its kind cannot be hurried.
    HurryQuote_t QuoteHurry() const;

    // Spends up to energyCredits on whole minerals at QuoteHurry's price, completing if the
    // stockpile then meets cost. Throws on bad credits, an unhurriable item or a short treasury.
    HurryResult_t HurryProduction(int energyCredits);

    // Collects nutrients and minerals and splits energy, once per turn in ResourceCollection.
    void ProduceResources();


    // Constructed-facility energy upkeep for this base (FacilityEnergyUpkeep mods applied).
    std::vector<BuildingUpkeepLine_t> GetBuildingUpkeepByType() const;
    int GetBuildingUpkeep() const;

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    // GrowthRate modifiers resolve against the same memoized base effect list.
    void ApplyGrowth();

    // Faction-wide and this base's own ThisBase rating modifiers, from the local pool only.
    int GetEffectiveSocialRating(SocialRatingId_t rating) const;

    int GetNutrientsRequired() const;

    Tile& GetTile();
    const Tile& GetTile() const;

    // Final effect list this base resolves against (faction pool + pops + SE rating expansion).
    const BaseEffects_t& GetBaseEffects() const;

    // Percentage chance of a fungal pop in this base's radius (see BaseEcology). Requires the
    // owning faction to be bound to a GameState.
    int GetEcologicalDamage() const;

    // Worked / preview tile yield after base-wide (and, for worked tiles, pop) effects.
    // .effective honours MaxClamp; .potential is uncapped (for UI).
    TileYieldView_t GetWorkedTileYield(const Tile& rTile) const;
    TileYieldView_t GetPreviewTileYield(const Tile& rTile) const;

    // Access to the tile-effects resolver (bundles WorldMap + ImprovementRegistry).
    // Used by BaseWorkableAreaDisplay and any other system that needs to resolve tile yield or defense.
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    const std::string& GetName() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;
    FactionId_t GetFactionId() const;
    int GetBaseId() const;

    // Units that claim this base as home (see HomeBaseIndex / HomeBaseClaim).
    HomeBaseIndex& GetHomeUnits();
    const HomeBaseIndex& GetHomeUnits() const;

    // For ownership transfer: the same base, re-pointed at the new owner's research, economy and
    // effects. The caller recalculates composition and workers.
    void RebindFaction(Faction& rFaction);

    // Once, when the base leaves the game (raze, or destruction), while still valid. Not on
    // transfer.
    Signal<> OnDestroyed;

    // Out of the game, but alive until the next ReapRazedBases.
    bool IsRazed() const;


    // oncePer keys this base has used; authored, so a rule can span config entries.
    std::set<std::string>& ConsumedTriggerKeys() { return m_consumedTriggerKeys; }
    const std::set<std::string>& ConsumedTriggerKeys() const { return m_consumedTriggerKeys; }

private:
    friend class Faction;

    // Leave the game without being destroyed: release the tile and emit OnDestroyed. Only
    // Faction::RazeBase calls this, and only once — it throws on a second call.
    void MarkRazed_();

    std::set<std::string> m_consumedTriggerKeys;
    // Rebindable owner (RebindFaction): never null while the base lives in a Faction.
    Faction* m_pFaction;
    int m_baseId;
    Tile& m_tile;
    TileEffectsContext& m_rTileEffects;
    // The base's own tile, claimed for life so no other base can work it.
    WorkedTileClaim m_centerTileClaim;
    // Units that call this base home. Destroyed with the base; orphans outstanding claims
    // so units never keep a dangling BaseManager*.
    HomeBaseIndex m_homeUnits;
    const BuildingRegistry& m_rBuildingRegistry;
    // Declared before m_pProduction: the default-item provider handed to ProductionManager
    // reads this registry while that member is being constructed.
    const StockpileRegistry& m_rStockpileRegistry;
    const SocialRatingRegistry& m_rSocialRatings;
    const HurryProductionCalculator& m_rHurryCalculator;
    const ScrapRefundCalculator& m_rScrapCalculator;
    const ProductionConfig_t& m_rProductionConfig;
    // Always the owning faction; a pointer only to be rebindable.
    // Declared after workers and buildings, which resources depends on.
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<BuildingManager> m_pBuildings;
    std::unique_ptr<ResourceManager> m_pResources;
    std::unique_ptr<ProductionManager> m_pProduction;
    std::string m_name;

    // What the queued item still needs, shared by the quote and the spend.
    HurryInputs_t BuildHurryInputs_(const IConstructable& rItem) const;

    // Per-copy price and destroy. Faction::QuoteScrapBuilding / ScrapBuilding are the
    // player-order entry; these exist so the empire-wide pair can reuse the same path.
    std::optional<ScrapPayout_t> QuoteScrapBuilding_(const BuildingId_t& buildingId) const;
    int ScrapBuilding_(const BuildingId_t& buildingId);

    void OnBuildingDestroyed_(const BuildingConfig_t& rBuilding);
    // Growth, starvation and the base's own pop and riot signals.
    void WirePopulation_();
    void ConnectSignals_();
    // What production builds when the queue is empty.
    const IConstructable* FallbackProduction_() const;
    // A finished building joins the base; a finished design becomes a unit here.
    void OnItemCompleted_(const std::string& itemId);
    void CompleteBuilding_(const std::string& itemId);
    void CompleteUnit_(const IDesign& rDesign, const std::string& itemId);
    // Current production and queue, re-pointed at what the new owner can build.
    void RehomeProduction_(const Faction& rFaction);
    const IConstructable* RehomeItem_(const IConstructable* pItem, const Faction& rFaction) const;

    // Assembles and memoizes the effect list this base resolves against.
    BaseEffectsCache m_effects;
    BaseEcology m_ecology;
    // Set by MarkRazed_. Suppresses the destructor's OnDestroyed / tile release, which the
    // raze already performed.
    bool m_bRazed = false;
    // Sea colony pod founded this base on water. Pressure Dome is checked live, not stored here.
    bool m_bMayOccupyWater = false;
    bool m_bStolenResearch = false;
    bool m_bPlagueIntroduced = false;
};

} // namespace ac
