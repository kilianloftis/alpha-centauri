#pragma once

#include "game/IConstructable.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/TileEffectsContext.h"
#include "lib/Signal.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class Pop;
class PopulationManager;
class PopContainer;
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
struct GameDataContext;

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers.
// Routes API calls to PopulationManager, ResourceManager, and WorkerAssignmentManager.
class BaseManager
{
public:
    BaseManager(
        Tile& tile,
        const GameDataContext& rDataContext,
        TileEffectsContext& rTileEffects,
        const ResearchManager* pResearchManager,
        const EconomyManager* pEconomyManager,
        const IEffectsProvider* pEffectsProvider = nullptr);
    ~BaseManager();

    // Population management - delegated to PopulationManager
    void RecalculatePopComposition();
    const PopContainer& GetPopContainer() const;
    int GetPopWorkerCount() const;
    void ConvertPop(Pop& rPop, const std::string& typeId);
    const std::string& GetDefaultWorkerTypeId() const;

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

    // Auto-assign all unassigned workers to available workable tiles.
    // Should be called after initial population setup or when new workers need assignment.
    void AutoAssignWorkers();

    // Resource production per turn (calculated live).
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEconProduction() const;
    int GetLabsProduction() const;
    int GetPsychProduction() const;

    // Consume the full accumulated resource stockpile, returning the amount consumed.
    int ConsumeEcon();
    int ConsumeLabs();
    int ConsumePsych();

    // Building management - delegated to BuildingManager
    void AddBuilding(const std::string& buildingId);
    void DestroyBuilding(const std::string& buildingId);
    const std::vector<const BuildingConfig_t*>& GetBuildings() const;
    std::vector<ActiveEffect_t> CollectBuildingEffects() const;
    std::vector<const IConstructable*> GetConstructable() const;

    // Production management - delegated to ProductionManager
    void SetProduction(const IConstructable* pItem);
    const IConstructable* GetCurrentProduction() const;
    int GetProductionMineralCost() const;
    int GetMineralStockpile() const;

    // Collect minerals from ResourceManager and apply to production this turn.
    // Completes construction if the stockpile meets the cost.
    // Returns the completed item id, or empty string if construction is ongoing.
    std::string ApplyProduction();

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

    int GetNutrientStockpile() const;
    int GetNutrientsRequired() const;
    int GetBaseSize() const;

    int GetX() const;
    int GetY() const;

    // Returns the set of workable tiles this base can assign workers to.
    const std::vector<const Tile*>& GetWorkableTilePositions() const;

    // Effective yield from a worked tile after base-wide and pop tile effects.
    // Unworked tiles should use TileEffectsContext::ResolveTileYield for intrinsic preview.
    TileResources_t GetWorkedTileYield(const Tile& rTile) const;

    // Access to the tile-effects resolver (bundles WorldMap + ImprovementRegistry).
    // Used by BaseWorkableAreaDisplay and any other system that needs to resolve tile yield or defense.
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    // Base identity
    void SetName(const std::string& name);
    const std::string& GetName() const;

    // Ownership
    void SetFactionId(FactionId factionId);
    FactionId GetFactionId() const;
    void SetBaseId(int baseId);
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
    BaseEffects_t BuildBaseEffects_() const;

    FactionId m_factionId;
    int m_baseId;
    Tile& m_tile;
    TileEffectsContext& m_rTileEffects;
    const BuildingRegistry* m_pBuildingRegistry;
    const SocialRatingRegistry* m_pSocialRatings;
    const ResearchManager* m_pResearch;
    const IEffectsProvider* m_pEffectsProvider = nullptr;
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<ResourceManager> m_pResources;
    std::unique_ptr<BuildingManager> m_pBuildings;
    std::unique_ptr<ProductionManager> m_pProduction;
    std::string m_name;
};

} // namespace ac
