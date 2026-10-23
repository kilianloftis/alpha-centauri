#pragma once

#include <set>

#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include "game/effects/IEffectsProvider.h"
#include "game/effects/IWorldEffectsSource.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingUpkeep.h"
#include "game/base/BaseTypes.h"
#include "game/base/BaseManager.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionEffectsPool.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/FactionRevealedUnits.h"
#include "game/visibility/FactionTileMemory.h"
#include "game/visibility/FactionVisibleMap.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/Unit.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include "game/effects/ActiveEffect.h"

namespace ac
{

class BuildingRegistry;
class TechRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class TechCostCalculator;
class FactionIdentity;
class AIProfile;
class FactionFlavor;
class EconomyManager;
class CommerceCache;
class Military;
class ResearchManager;
class ResearchSelector;
class SocialEngineeringManager;
class PopTypeAvailabilityCalculator;
class UnitManager;
class UnitPositionIndex;
class MoraleCalculator;
struct PopTypeConfig_t;
struct GameDataContext;
class WorldMap;
class GameSettings;
class GameState;

// Free rather than a member, so both Bases() overloads can deduce their return type from it.
template <typename Vec>
auto LiveBasesView(Vec& rBases)
{
    return DerefView(rBases)
        | std::views::filter([](const BaseManager& rBase) { return !rBase.IsRazed(); });
}

class Faction : public IEffectsProvider
{
public:
    // rDataContext must outlive the faction. rWorldMap sizes the fog maps; GameState::AddFaction
    // adds the session wiring. seed drives every per-faction random choice.
    Faction(FactionId_t factionId,
             bool bIsPlayerControlled,
             const FactionConfig_t& rDefinition,
             const GameDataContext& rDataContext,
             WorldMap& rWorldMap,
             const GameSettings& rSettings,
             uint32_t seed);
    ~Faction();

    FactionId_t GetFactionId() const { return m_factionId; }
    // What GameState::GetPlayerFaction() searches for.
    bool IsPlayerControlled() const { return m_bIsPlayerControlled; }

    const FactionConfig_t& GetDefinition() const { return m_rDefinition; }
    const std::string& GetDefinitionId() const { return m_rDefinition.id; }

    std::string SuggestBaseName();

    void AddBase(std::unique_ptr<BaseManager> pBase);
    // Destroys the base and returns its snapshot; home claims are orphaned. Ownership changes
    // that keep the base use TransferBaseTo.
    std::optional<BaseSnapshot_t> ExtractBase(BaseId_t baseId);
    // Move a base out of this faction's ownership without destroying it (identity-preserving
    // transfer's building block). Throws if baseId is missing.
    std::unique_ptr<BaseManager> ReleaseBase(BaseId_t baseId);
    // Rebuilds a base from a snapshot as a new object, for save/load. Not a transfer.
    BaseManager* CreateBaseFromSnapshot(const BaseSnapshot_t& rSnapshot,
                                        TileEffectsContext& rTileEffects);
    // Moves the same base object to rReceiver, recalculating its pops and workers and migrating
    // its deploy cooldowns. Throws if baseId is missing or rReceiver is this faction.
    void TransferBaseTo(BaseId_t baseId, Faction& rReceiver);
    // Builds a base from the faction's ruleset; a faction without a Headquarters gets one there.
    BaseManager* CreateBase(BaseId_t baseId, const std::string& name, Tile* pTile,
                            TileEffectsContext& rTileEffects,
                            std::optional<int> initialPopulation);
    // Live bases. Razed ones are filtered here, not erased, so a base can die mid-iteration; the
    // object lasts until ReapRazedBases.
    auto Bases() { return LiveBasesView(m_bases); }
    auto Bases() const { return LiveBasesView(m_bases); }
    size_t GetBaseCount() const;

    // Live base with this id, or nullptr. A razed base is already gone as far as this answers.
    BaseManager* FindBase(BaseId_t baseId);
    const BaseManager* FindBase(BaseId_t baseId) const;

    // Takes rBase out of the game now; only the object's destruction waits for ReapRazedBases.
    // Idempotent. Throws if rBase is not this faction's.
    void RazeBase(BaseManager& rBase);

    // Frees bases razed since the last call, between turn stages.
    void ReapRazedBases();

    // At the end of every AddBase: founding, load and transfer alike.
    Signal<BaseManager&> OnBaseAdded;

    // After any base-list change, before the visibility rebuild on add.
    Signal<> OnBaseListChanged;

    // Fired at the end of RebuildVisibility. GameState uses this for first-contact scans.
    Signal<Faction&> OnVisibilityRebuilt;

    // Sum of population size across all bases.
    int TotalPopulation() const;

    // Base with the Headquarters RuleFlag, or nullptr if none.
    BaseManager* GetHeadquarters();
    const BaseManager* GetHeadquarters() const;

    // Base closest to rFrom by wrap-aware Chebyshev distance, or nullptr when the faction holds
    // none. Ties break on Bases() order, so the answer is stable for a given base list.
    BaseManager* FindNearestBase(const Tile& rFrom);
    const BaseManager* FindNearestBase(const Tile& rFrom) const;


    // Sum of constructed copies of buildingId across all bases.
    int CountBuildings(const BuildingId_t& buildingId) const;
    // First base that holds at least one copy, or nullptr.
    BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId);
    const BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId) const;
    // Config of any owned copy, or nullptr when the faction has none.
    const BuildingConfig_t* FindOwnedBuildingConfig(const BuildingId_t& buildingId) const;
    // Copies not on deploy cooldown at missionYear. A pure query; PruneExpiredDeploys retires
    // expired records.
    int CountReadyBuildings(const BuildingId_t& buildingId, int missionYear) const;
    // Retires records ready at missionYear, so the ledger cannot grow without bound. Changes no
    // CountReadyBuildings answer.
    void PruneExpiredDeploys(int missionYear);
    // Marks one copy at baseId deployed until readyMissionYear. Keyed by base, so records move or
    // drop with the base.
    void DeployBuilding(BaseId_t baseId, const BuildingId_t& buildingId, int readyMissionYear);
    // Drops the record for the copy at baseId; a no-op if it was not cooling.
    void NotifyBuildingDestroyed(BaseId_t baseId, const BuildingId_t& buildingId);

    // Fired when a secret project is permanently lost (destroyed or razed with the base).
    // GameState tombstones the id so it cannot be rebuilt.
    Signal<const BuildingId_t&> OnSecretProjectDestroyed;

    // Economy subsystem: energy treasury and allocation split.
    EconomyManager& GetEconomy();
    const EconomyManager& GetEconomy() const;

    // Commerce subsystem: Treaty/Pact pairing queries for produce, net income, and UI.
    CommerceCache& GetCommerce();
    const CommerceCache& GetCommerce() const;

    // Consume every base's accumulated econ stockpile into the treasury.
    // Returns the amount collected this call.
    int CollectIncome();

    // Consume every base's accumulated labs stockpile into research points.
    // Returns the amount collected this call.
    int CollectResearch();

    // Projected per-turn outputs summed across all bases.
    int GetNetIncomePerTurn() const;
    std::optional<int> GetTurnsUntilBreakthrough() const;

    // Each base resolves its resources against the composed pool, applies inefficiency and splits
    // energy. CollectIncome and CollectResearch gather econ and labs.
    void ProduceBaseResources();

    // Mineral support at every base, between ResourceCollection and BaseProduction.
    void ApplyMineralSupport();

    // Sum of BuildingConfig_t::upkeep across every constructed building copy (all bases).
    // Continuous GrantBuilding expansions are not constructed and are not included.
    int GetBuildingUpkeep() const;

    // Constructed buildings across all bases, grouped by type for UI (name, count, upkeep).
    std::vector<BuildingUpkeepLine_t> GetBuildingUpkeepByType() const;

    // Pays GetBuildingUpkeep() from the treasury, after IncomeCollection. Throws if it cannot
    // (no auto-sell yet).
    void ApplyBuildingUpkeep();

    // Apply growth to all bases, incorporating GrowthRate stat effects.
    void ApplyBaseGrowth();

    // Research subsystem.
    ResearchManager& GetResearch();
    const ResearchManager& GetResearch() const;

    // Discover the current research target and auto-select the next one.
    bool DiscoverCurrentResearch();

    // Assign a research target when none is set (grants/steals while idle, startup).
    void EnsureResearchTarget();

    // Social engineering subsystem.
    SocialEngineeringManager& GetSocialEngineering();
    const SocialEngineeringManager& GetSocialEngineering() const;

    // Military (unit designs and units)
    Military& GetMilitary();
    const Military& GetMilitary() const;

    // Live units owned by this faction.
    UnitManager& GetUnitManager();
    const UnitManager& GetUnitManager() const;

    // Moves the unit to rReceiver without destroying it. Cargo travels with a carrier; an
    // embarked passenger detaches; the home claim clears. Throws if missing or self.
    void TransferUnitTo(UnitId_t unitId, Faction& rReceiver);

    // What scrapping one copy at rBase would pay, and where. Empty when it cannot be scrapped.
    // Throws if rBase is not this faction's.
    std::optional<ScrapPayout_t> QuoteScrapBuilding(const BaseManager& rBase,
                                                   const BuildingId_t& buildingId) const;

    // Player scrap only: destroys one copy and pays QuoteScrapBuilding. Throws if the quote is
    // empty.
    int ScrapBuilding(BaseManager& rBase, const BuildingId_t& buildingId);

    // QuoteScrapBuilding summed over every copy; destBaseId only when all pay into one sink.
    std::optional<ScrapPayout_t> QuoteScrapBuildings(const BuildingId_t& buildingId) const;

    // Player scrap only: ScrapBuilding for every copy. Throws if the quote is empty.
    int ScrapBuildings(const BuildingId_t& buildingId);

    // What scrapping the unit would pay: to the closest friendly base, or 0 off territory. Empty
    // without a default_scrap.
    std::optional<ScrapPayout_t> QuoteScrapUnit(const Unit& rUnit) const;

    // Player scrap only: destroys the unit and pays QuoteScrapUnit. Throws if the quote is empty.
    int ScrapUnit(Unit& rUnit);

    // Explored memory and current vision. RebuildVisibility refreshes vision when units are
    // created, destroyed or moved and when bases are added.
    FactionExploredMap& GetExploredMap();
    const FactionExploredMap& GetExploredMap() const;
    FactionVisibleMap& GetVisibleMap();
    const FactionVisibleMap& GetVisibleMap() const;
    // What the faction last saw on each tile; tiles out of sight draw from it.
    FactionTileMemory& GetTileMemory();
    const FactionTileMemory& GetTileMemory() const;
    // Contact reveal: concealed units this faction has bumped into (occupied tile / ZOC).
    FactionRevealedUnits& GetRevealedUnits();
    const FactionRevealedUnits& GetRevealedUnits() const;
    const WorldMap& GetWorldMap() const { return m_rWorldMap; }
    void RebuildVisibility();

    // Coalesces rebuilds: the outermost scope rebuilds once, so a burst of unit events (a sunk
    // transport) does not pay for a full rebuild and contact sweep each.
    class VisibilityRebuildScope
    {
    public:
        VisibilityRebuildScope(const VisibilityRebuildScope&) = delete;
        VisibilityRebuildScope& operator=(const VisibilityRebuildScope&) = delete;
        VisibilityRebuildScope(VisibilityRebuildScope&& rOther) noexcept;
        VisibilityRebuildScope& operator=(VisibilityRebuildScope&&) = delete;
        ~VisibilityRebuildScope();

    private:
        friend class Faction;
        explicit VisibilityRebuildScope(Faction& rFaction);

        Faction* m_pFaction;
    };

    VisibilityRebuildScope DeferVisibilityRebuild();

    // Session prefs (Engine/GameState owned). Used by RebuildVisibility to apply
    // GameRules.RemoveShroud / DebugOptions.RemoveFog without compiling them in.
    const GameSettings& GetSettings() const { return m_rSettings; }

    // Required to complete unit production.
    // TODO: the faction's last link to its GameState; allocate unit ids through WorldMap instead?
    void BindUnitSpawnServices(std::function<UnitId_t()> allocateUnitId,
                               UnitPositionIndex& rPositions);
    UnitId_t AllocateUnitId() const;
    UnitPositionIndex& GetUnitPositions();

    // Sticky fog removal from ApplyRemoveFog (one-shot project completion). Continuous
    // RuleFlag / debug settings are layered on top in ApplyVisibilityRules.
    void SetFogRemoved(bool bFogRemoved) { m_bFogRemoved = bFogRemoved; }
    bool IsFogRemoved() const { return m_bFogRemoved; }

    std::vector<const PopTypeConfig_t*> GetAvailablePopTypes() const;

    // IEffectsProvider — composed view: local FactionEffectsPool plus bound world extras.
    const FactionEffects_t& GetActiveEffects() const override;
    uint64_t GetEffectsVersion() const override;

    // Own contributions only, for peers' world composition and faction-internal social ratings.
    const FactionEffects_t& GetLocalActiveEffects() const override;

    // Grids the pooled effects override; with the design's mask, decides whether a resolve
    // collects effects.
    InteractionGridMask_t GetInteractionMask() const;
    uint64_t GetLocalEffectsVersion() const;


    // oncePer keys this faction has used; authored, so a rule can span config entries.
    std::set<std::string>& ConsumedTriggerKeys() { return m_consumedTriggerKeys; }
    const std::set<std::string>& ConsumedTriggerKeys() const { return m_consumedTriggerKeys; }

private:
    void EnsureComposedEffects_() const;
    int GetResearchPerTurn_() const;

    std::set<std::string> m_consumedTriggerKeys;
    FactionId_t m_factionId;
    bool m_bIsPlayerControlled;
    const FactionConfig_t& m_rDefinition;
    const GameDataContext& m_rDataContext;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<FactionFlavor> m_pFlavor;
    std::unique_ptr<EconomyManager> m_pEconomy;
    std::unique_ptr<CommerceCache> m_pCommerce;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<ResearchManager> m_pResearch;
    std::unique_ptr<ResearchSelector> m_pResearchSelector;
    std::unique_ptr<SocialEngineeringManager> m_pSocialEngineering;
    // Bases before units: ~Faction destroys units first so Unit can release crawler claims
    // through WorkerAssignmentManager while the home base is still alive.
    std::vector<std::unique_ptr<BaseManager>> m_bases;
    std::unique_ptr<UnitManager> m_pUnits;
    Revision m_baseListRevision; // bumped when a base is added (later: removed/captured)
    // Declared after m_baseListRevision, which its constructor binds a reference to.
    FactionEffectsPool m_effectsPool;
    FactionExploredMap m_explored;
    FactionVisibleMap m_visible;
    FactionTileMemory m_tileMemory;
    FactionRevealedUnits m_revealedUnits;
    WorldMap& m_rWorldMap; // constructor-injected; RebuildVisibility can never be a no-op
    const GameSettings& m_rSettings; // non-owning session prefs; constructor-injected
    std::function<UnitId_t()> m_allocateUnitId;
    UnitPositionIndex* m_pUnitPositions = nullptr;
    bool m_bFogRemoved = false; // sticky ApplyRemoveFog
    int m_visibilityDeferralDepth = 0;
    bool m_bVisibilityDirty = false;

    // Composed GetActiveEffects cache (local pool + world extras). The two stamps are the
    // cache key; m_composedVersion is what GetEffectsVersion publishes.
    mutable FactionEffects_t m_composedEffects;
    mutable uint64_t m_composedLocalVersion = UINT64_MAX;
    mutable uint64_t m_composedWorldStamp = UINT64_MAX;
    mutable const IWorldEffectsSource* m_pComposedWorld = nullptr;
    mutable uint64_t m_composedVersion = 0;
    // Recomputed with m_composedEffects, so it can never go stale independently.
    mutable InteractionGridMask_t m_composedInteractionMask = 0;

    // Moves baseId's deploy records to rReceiver, so cooldowns follow the base.
    void MigrateBuildingDeploys_(BaseId_t baseId, Faction& rReceiver);
    // Drop every deploy record for baseId (base destroyed: razed on capture).
    void DropBuildingDeploys_(BaseId_t baseId);
    // Revision, observers and visibility follow every change to the base list.
    void NoteBaseListChanged_();
    // Each snapshot production id as a building, stockpile or design; throws on an unknown one.
    std::vector<const IConstructable*> ResolveProductionItems_(
        const std::vector<std::string>& rItemIds) const;

    // ASAT and intercept cooldowns, keyed by (base, building); unavailable while missionYear <
    // readyMissionYear.
    struct BuildingDeploy_t
    {
        BaseId_t baseId = 0;
        BuildingId_t buildingId;
        int readyMissionYear = 0;
    };
    std::vector<BuildingDeploy_t> m_buildingDeploys;
};

} // namespace ac
