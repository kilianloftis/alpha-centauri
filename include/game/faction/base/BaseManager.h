#pragma once

#include "game/IConstructable.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/BaseTypes.h"
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
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
struct GrowthConfig_t;
class PopCompositionCalculator;
class SecretProjectAvailabilityCalculator;

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers. Sub-managers own their full
// API surface; a method lives here only when it coordinates two or more subsystems
// (e.g. production consuming resources, growth resolving faction effects).
class BaseManager
{
public:
    // pEffectsProvider is the owning faction's effect pool; may be null for a base that
    // resolves no faction effects (standalone test bases). pPopTypeRegistry,
    // pPopTypeAvailabilityCalculator, pGrowthConfig, and pCompositionCalculator are forwarded
    // to the population subsystem; pSecretProjectCalculator is forwarded to the building
    // subsystem. All are named individually (rather than taking a whole GameDataContext) so
    // this leaf class's real dependencies are visible at the call site; the caller (a factory
    // such as Faction::CreateBase) is responsible for unpacking them.
    BaseManager(
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
        const IEffectsProvider* pEffectsProvider);
    ~BaseManager();

    // Population subsystem. Pure population queries and mutations go through this reference;
    // BaseManager only keeps operations that coordinate population with other subsystems.
    PopulationManager& GetPopulation();
    const PopulationManager& GetPopulation() const;

    // Convert a pop while keeping worker-tile assignments consistent: unassigns the pop's
    // tile before converting and re-runs auto-assignment if it is a worker afterwards.
    void ConvertPop(Pop& rPop, const std::string& typeId);

    // Signals forwarded from PopulationManager
    Signal<int> on_pop_gained;
    Signal<int> on_pop_lost;

    // Signals forwarded from ProductionManager
    Signal<std::string> on_production_completed;

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
    void ProduceResources(const FactionEffects_t& rFactionEffects);

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    // rFactionEffects is the faction-wide pool; GrowthRate modifiers are resolved per base.
    void ApplyGrowth(const FactionEffects_t& rFactionEffects);

    // This base's effective social rating on one axis: faction-wide SocialRatingModifier
    // contributions plus any ThisBase-scoped ones originating here (e.g. a building's
    // +1 Growth).
    int GetEffectiveSocialRating(SocialRatingId rating) const;

    int GetNutrientsRequired() const;

    int GetX() const;
    int GetY() const;

    // Effective yield from a worked tile after base-wide and pop tile effects.
    // Unworked tiles should use TileEffectsContext::ResolveTileYield for intrinsic preview.
    TileResources_t GetWorkedTileYield(const Tile& rTile) const;

    // Access to the tile-effects resolver (bundles WorldMap + ImprovementRegistry).
    // Used by BaseWorkableAreaDisplay and any other system that needs to resolve tile yield or defense.
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    // Base identity
    const std::string& GetName() const;
    FactionId GetFactionId() const;
    int GetBaseId() const;

private:
    // FilterForBase over the faction-wide pool, plus this base's own pop-generated ThisBase
    // effects — everything that applies to this base, before rating expansion. Shared by
    // BuildBaseEffects_ (which expands ratings into gameplay effects) and GetEffectiveSocialRating
    // (which only accumulates the rating totals).
    BaseEffects_t CollectBaseLocalEffects_(const FactionEffects_t& rFactionEffects) const;

    // The final effect list this base resolves against: CollectBaseLocalEffects_ plus the
    // gameplay effects of this base's effective social rating levels
    // (ExpandSocialRatingEffects).
    BaseEffects_t BuildBaseEffects_(const FactionEffects_t& rFactionEffects) const;

    // Memoized variant over the provider's pool: rebuilt only when the provider's effects
    // version changed. The reference is valid until the next effect-source mutation.
    const BaseEffects_t& BuildBaseEffects_() const;

    FactionId m_factionId;
    int m_baseId;
    Tile& m_tile;
    TileEffectsContext& m_rTileEffects;
    // The base works its own tile for free (no pop), so it holds the tile's claim in the
    // world WorkedTileIndex for its whole life — no other base, friendly or enemy, can
    // ever work another base's tile. Released automatically when the base is destroyed.
    WorkedTileClaim m_centerTileClaim;
    const BuildingRegistry* m_pBuildingRegistry;
    const SocialRatingRegistry* m_pSocialRatings;
    const ResearchManager* m_pResearch;
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
