#pragma once

#include <memory>
#include <optional>
#include <functional>
#include <vector>

#include "game/IEffectsProvider.h"
#include "game/faction/base/BaseTypes.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionEffectsPool.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"

namespace ac
{

// Forward declarations
class BuildingRegistry;
class TechRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class TechCostCalculator;
class FactionIdentity;
class AIProfile;
class FactionFlavor;
class EconomyManager;
class Military;
class ResearchManager;
class ResearchSelector;
class SocialEngineeringManager;
class PopTypeAvailabilityCalculator;
class SecretProjectAvailabilityCalculator;
class UnitManager;
struct PopTypeConfig_t;
struct GameDataContext;
class WorldMap;

class Faction : public IEffectsProvider
{
public:
    Faction(FactionId_t factionId,
             bool bIsPlayerControlled,
             const FactionConfig_t& rDefinition,
             const BuildingRegistry* pBuildingRegistry,
             const TechRegistry* pTechRegistry,
             const SocialPolicyRegistry* pSocialPolicyRegistry,
             const SocialRatingRegistry* pSocialRatingRegistry,
             TechCostCalculator* pTechCostCalculator,
             const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator);
    ~Faction();

    FactionId_t GetFactionId() const { return m_factionId; }
    // Not multiplayer yet, but the flag (rather than an index-0 convention) is what
    // GameState::GetPlayerFaction() searches for, so it generalizes to multiple
    // human-controlled factions without a representation change.
    bool IsPlayerControlled() const { return m_bIsPlayerControlled; }

    const FactionConfig_t& GetDefinition() const { return m_rDefinition; }
    const std::string& GetDefinitionId() const { return m_rDefinition.id; }

    std::string SuggestBaseName();

    // Base management
    void AddBase(std::unique_ptr<BaseManager> pBase);
    // Destroy the base and return a snapshot for reconstruct-under-new-owner.
    std::optional<BaseSnapshot_t> ExtractBase(BaseId_t baseId);
    // Rebuild a base from a snapshot (recalculates pop roles and worked tiles).
    BaseManager* CreateBaseFromSnapshot(const BaseSnapshot_t& rSnapshot,
                                        const GameDataContext& rDataContext,
                                        TileEffectsContext& rTileEffects,
                                        const SecretProjectAvailabilityCalculator& rSecretProjectAvailability);
    // Extract from this faction and CreateBaseFromSnapshot on rReceiver.
    // Throws if baseId is missing or rReceiver is this faction.
    void TransferBaseTo(BaseId_t baseId,
                        Faction& rReceiver,
                        const GameDataContext& rDataContext,
                        TileEffectsContext& rTileEffects,
                        const SecretProjectAvailabilityCalculator& rSecretProjectAvailability);
    // Factory method: unpacks the individual registries/calculators BaseManager needs from
    // rDataContext (a composition-root-supplied bag) so BaseManager itself can declare narrow,
    // named dependencies instead of taking the whole context.
    BaseManager* CreateBase(BaseId_t baseId, const std::string& name, Tile* pTile,
                            const GameDataContext& rDataContext,
                            TileEffectsContext& rTileEffects,
                            const SecretProjectAvailabilityCalculator& rSecretProjectAvailability);
    // Iterate bases by reference without exposing the owning unique_ptrs.
    auto Bases() { return DerefView(m_bases); }
    auto Bases() const { return DerefView(m_bases); }
    size_t GetBaseCount() const { return m_bases.size(); }

    // Returns buildings the faction has the technology to build.
    std::vector<const BuildingConfig_t*> GetDiscoveredBuildings() const;

    // Economy subsystem: energy treasury and allocation split.
    EconomyManager& GetEconomy();
    const EconomyManager& GetEconomy() const;

    // Consume every base's accumulated econ stockpile into the treasury.
    // Returns the amount collected this call.
    int CollectIncome();

    // Consume every base's accumulated labs stockpile into research points.
    // Returns the amount collected this call.
    int CollectResearch();

    // Projected per-turn outputs summed across all bases.
    int GetNetIncomePerTurn() const;
    std::optional<int> GetBreakthroughRate() const;
    std::optional<int> GetTurnsUntilBreakthrough() const;

    // Resource production - routes to all bases and faction economy manager.
    // rExternalEffects are effects from outside this faction (other factions' WorldGlobal
    // effects, gathered by the turn stage via CollectWorldEffects); appended to the pool.
    void ProduceBaseResources(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Apply growth to all bases, incorporating GrowthRate stat effects.
    void ApplyBaseGrowth(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Research subsystem.
    ResearchManager& GetResearch();
    const ResearchManager& GetResearch() const;

    // Discover the current research target and auto-select the next one.
    bool DiscoverCurrentResearch();

    // Social engineering subsystem.
    SocialEngineeringManager& GetSocialEngineering();
    const SocialEngineeringManager& GetSocialEngineering() const;

    // Military (unit designs and units)
    Military& GetMilitary();
    const Military& GetMilitary() const;

    // Live units owned by this faction.
    UnitManager& GetUnitManager();
    const UnitManager& GetUnitManager() const;

    // Fog of war: permanent explored memory and currently-visible tiles as separate maps.
    // BindWorldMap sizes both from the shared WorldMap; RebuildVisibility refreshes
    // current vision from units/bases (and grows explored). Unit create/destroy call it
    // from UnitManager; moves reach it via UnitPositionIndex::OnUnitMoved (wired by
    // GameState); base founding invokes it from AddBase.
    FactionExploredMap& GetExploredMap();
    const FactionExploredMap& GetExploredMap() const;
    FactionVisibleMap& GetVisibleMap();
    const FactionVisibleMap& GetVisibleMap() const;
    // Contact reveal: concealed units this faction has bumped into (occupied tile / ZOC).
    FactionRevealedUnits& GetRevealedUnits();
    const FactionRevealedUnits& GetRevealedUnits() const;
    void BindWorldMap(WorldMap& rWorldMap);
    void RebuildVisibility();

    // Invoked after AddBase (after visibility rebuild). GameState uses this to rebuild
    // world territory; tests may leave it unset and call TerritoryMap::Rebuild directly.
    void SetOnBaseListChanged(std::function<void()> handler);

    // Invoked at the end of RebuildVisibility. GameState uses this for first-contact scans.
    void SetOnVisibilityRebuilt(std::function<void(Faction&)> handler);

    // Pop types
    std::vector<const PopTypeConfig_t*> GetAvailablePopTypes() const;

    // IEffectsProvider — delegates to the memoized FactionEffectsPool component.
    const FactionEffects_t& GetActiveEffects() const override;
    uint64_t GetEffectsVersion() const override;

private:
    int GetResearchPerTurn_() const;

    FactionId_t m_factionId;
    bool m_bIsPlayerControlled;
    const FactionConfig_t& m_rDefinition;
    const BuildingRegistry* m_pBuildingRegistry;
    const PopTypeAvailabilityCalculator* m_pPopTypeAvailabilityCalculator;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<FactionFlavor> m_pFlavor;
    std::unique_ptr<EconomyManager> m_pEconomy;
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
    FactionRevealedUnits m_revealedUnits;
    WorldMap* m_pWorldMap = nullptr; // set by BindWorldMap; used by RebuildVisibility
    std::function<void()> m_onBaseListChanged;
    std::function<void(Faction&)> m_onVisibilityRebuilt;
};

} // namespace ac
