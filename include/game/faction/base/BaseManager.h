#pragma once

#include "game/IConstructable.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/BaseTypes.h"
#include "game/faction/base/HomeBaseIndex.h"
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

// Forward declarations
class Pop;
class PopulationManager;
class WorkerAssignmentManager;
class EconomyManager;
class ResourceManager;
class BuildingManager;
class BuildingRegistry;
class ProductionManager;
class ResearchManager;
class SocialRatingRegistry;
class IEffectsProvider;
class Tile;
class Faction;
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
struct GrowthConfig_t;
class PopCompositionCalculator;
class SecretProjectAvailabilityCalculator;

using BaseId_t = int;

// Non-recomputable state needed to destroy a base and rebuild it under a new owner.
// Worked tiles and pop roles are omitted — recalculated after reconstruct (psych changes).
struct BaseSnapshot_t
{
    BaseId_t baseId = 0;
    std::string name;
    Tile* pTile = nullptr;
    int populationSize = 0;
    std::vector<std::string> buildingIds;
    // Empty when nothing is queued.
    std::string productionItemId;
    int mineralStockpile = 0;  // progress toward current production
    int nutrientStockpile = 0; // growth bank
};

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers. Sub-managers own their full
// API surface; a method lives here only when it coordinates two or more subsystems
// (e.g. production consuming resources, growth resolving faction effects).
class BaseManager
{
public:
    // rFaction is the owning faction; its lifetime must outlive this base.
    //
    // Narrow, named dependencies rather than the whole GameDataContext (Faction::CreateBase
    // unpacks them). They are references: the composition root always supplies them, and the
    // pointer form had three different null behaviours — throw, silent skip of all
    // social-rating expansion, and a no-op RecalculateComposition — which is how fixture bases
    // came to resolve ratings differently from real ones.
    //
    // Research manager, economy manager and effects provider are NOT parameters: they are the
    // owning faction's, and passing them separately let a caller inject one faction's economy
    // into another faction's base while RebindFaction silently overrode it on transfer.
    //
    // pSecretProjectCalculator is the one genuinely optional dependency: it reads live session
    // state (the faction vector) and so only exists inside a GameState. A base without one
    // cannot answer secret-project availability, and BuildingManager says so rather than
    // guessing.
    BaseManager(
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
        int initialPopulation = 3);
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

    // Signals forwarded from ProductionManager
    Signal<std::string> OnProductionCompleted;

    // Worker assignment - delegated to WorkerAssignmentManager
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // Find the best available pop and user-assign them to the given tile.
    // Priority: unassigned worker → specialist converted to worker → steal an assigned worker.
    // No-op if the tile is not workable or no pop can be found.
    void UserAssignBestAvailableWorker(const Tile* pTile);

    // Resource production per turn (calculated live).
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEconProduction() const;
    int GetLabsProduction() const;
    int GetPsychProduction() const;

    // Resource subsystem: per-turn stockpiles and their consumption.
    ResourceManager& GetResources();
    const ResourceManager& GetResources() const;

    // Building subsystem.
    BuildingManager& GetBuildingManager();
    const BuildingManager& GetBuildingManager() const;

    // This base's building effects, with ThisBase-scoped ones stamped with this base's identity.
    std::vector<ActiveEffect_t> CollectBuildingEffects() const;

    // Items this base can currently construct.
    std::vector<const IConstructable*> GetConstructable() const;

    // Production subsystem.
    ProductionManager& GetProduction();
    const ProductionManager& GetProduction() const;

    // Collect minerals from ResourceManager and apply to production this turn.
    // Completes construction if the stockpile meets the cost.
    // Returns the completed item id, or empty string if construction is ongoing.
    std::string ApplyProduction();

    // Effective mineral cost of the current production item after CostMultiplier effects
    // (e.g. Industry social-rating levels expanded into the base effect list).
    // Returns 0 when nothing is queued.
    int GetMineralCost() const;

    // Collect resources from worked tiles and allocate energy to categories.
    // Called once per turn per base during ResourceCollection stage.
    // Resolves against the composed provider pool (BuildBaseEffects_ memo).
    void ProduceResources();

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    // GrowthRate modifiers resolve against the same memoized base effect list.
    void ApplyGrowth();

    // This base's effective social rating on one axis: faction-wide SocialRatingModifier
    // contributions plus any ThisBase-scoped ones originating here (e.g. a building's
    // +1 Growth), accumulated from the faction's local pool only — peer WorldGlobal and
    // council effects do not move ratings.
    int GetEffectiveSocialRating(SocialRatingId_t rating) const;

    int GetNutrientsRequired() const;

    Tile& GetTile();
    const Tile& GetTile() const;

    // Final effect list this base resolves against (faction pool + pops + SE rating expansion).
    const BaseEffects_t& GetBaseEffects() const;

    // Worked / preview tile yield after base-wide (and, for worked tiles, pop) effects.
    // .effective applies TileResourceCap; .potential is uncapped (for UI).
    TileYieldView_t GetWorkedTileYield(const Tile& rTile) const;
    TileYieldView_t GetPreviewTileYield(const Tile& rTile) const;

    // Access to the tile-effects resolver (bundles WorldMap + ImprovementRegistry).
    // Used by BaseWorkableAreaDisplay and any other system that needs to resolve tile yield or defense.
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    // Base identity
    const std::string& GetName() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;
    FactionId_t GetFactionId() const;
    int GetBaseId() const;

    // Units that claim this base as home (see HomeBaseIndex / HomeBaseClaim).
    HomeBaseIndex& GetHomeUnits();
    const HomeBaseIndex& GetHomeUnits() const;

    // Ownership transfer (Faction::TransferBaseTo): rebind to the new owner in place — same
    // object, same baseId, same tile/HomeBaseIndex/WorkerAssignmentManager. Re-points every
    // per-faction dependency this base resolves through (effects provider, research, economy)
    // so production/tech-gated buildings and the econ split use the new owner from the next
    // resolve. Caller still owns recalculating composition/worker assignment afterward (psych
    // may differ under the new owner). See docs/architecture/high-level.md, "Object lifetime".
    void RebindFaction(Faction& rFaction);

    // Fired at the start of ~BaseManager, while the object is still fully valid, so observers
    // (e.g. an open BaseView) can invalidate their reference before it dangles. Not fired on
    // ownership transfer — the object survives that; see RebindFaction / GetFaction().
    Signal<> OnDestroyed;

private:
    // FilterForBase over a faction-wide pool, plus this base's own pop-generated ThisBase
    // effects — everything from that pool that applies to this base, before rating
    // expansion. Called with the composed pool for resolution (BuildBaseEffects_) and with
    // the local pool for ratings (CollectRatingSource_).
    BaseEffects_t CollectBaseLocalEffects_(const FactionEffects_t& rFactionEffects) const;

    // The final effect list this base resolves against: CollectBaseLocalEffects_ over the
    // composed pool, plus the gameplay effects of this base's effective social rating
    // levels — which are accumulated from the *local* pool only (ratings are a
    // faction-internal axis; see SocialRatingResolver).
    BaseEffects_t BuildBaseEffects_(const FactionEffects_t& rFactionEffects) const;

    // This base's rating context: CollectBaseLocalEffects_ over the provider's local pool.
    // The one input to both GetEffectiveSocialRating and the base-lane rating expansion,
    // so the number the UI reports and the effects the base resolves cannot disagree.
    BaseEffects_t CollectRatingSource_() const;

    // Memoized variant over the provider's pool: rebuilt only when the provider's effects
    // version changed. The reference is valid until the next effect-source mutation.
    const BaseEffects_t& BuildBaseEffects_() const;

    // Rebindable owner (RebindFaction): never null while the base lives in a Faction.
    Faction* m_pFaction;
    int m_baseId;
    Tile& m_tile;
    TileEffectsContext& m_rTileEffects;
    // The base works its own tile for free (no pop), so it holds the tile's claim in the
    // world WorkedTileIndex for its whole life — no other base, friendly or enemy, can
    // ever work another base's tile. Released automatically when the base is destroyed.
    WorkedTileClaim m_centerTileClaim;
    // Units that call this base home. Destroyed with the base; orphans outstanding claims
    // so units never keep a dangling BaseManager*.
    HomeBaseIndex m_homeUnits;
    const BuildingRegistry& m_rBuildingRegistry;
    const SocialRatingRegistry& m_rSocialRatings;
    // Always the owning faction (set in the ctor, re-pointed by RebindFaction). A pointer only
    // because it is rebindable; never null.
    const IEffectsProvider* m_pEffectsProvider = nullptr;
    // Declaration order is construction order: resources depends on worker assignments
    // and buildings, so it is declared after both.
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<BuildingManager> m_pBuildings;
    std::unique_ptr<ResourceManager> m_pResources;
    std::unique_ptr<ProductionManager> m_pProduction;
    std::string m_name;

    // Memoized BuildBaseEffects_ result, keyed on the provider's pool version
    // (empty = never built).
    mutable BaseEffects_t m_cachedBaseEffects;
    mutable std::optional<uint64_t> m_cachedPoolVersion;
};

} // namespace ac
