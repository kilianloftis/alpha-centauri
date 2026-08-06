#pragma once

#include <memory>
#include <optional>
#include <functional>
#include <vector>

#include "game/IEffectsProvider.h"
#include "game/IWorldEffectsSource.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/BaseTypes.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionEffectsPool.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/Unit.h"
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
class UnitPositionIndex;
class MoraleCalculator;
struct PopTypeConfig_t;
struct GameDataContext;
class WorldMap;
class GameSettings;
class GameState;

class Faction : public IEffectsProvider
{
public:
    // rDataContext is the game-wide definition data (registries plus the calculators built
    // from them). It is held by reference for the faction's whole life and handed to the
    // subsystems that need pieces of it, so adding a new kind of shared game data does not
    // change this signature. Must outlive the faction — see Engine's member ordering.
    Faction(FactionId_t factionId,
             bool bIsPlayerControlled,
             const FactionConfig_t& rDefinition,
             const GameDataContext& rDataContext);
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

    // Sum of population size across all bases.
    int TotalPopulation() const;

    // Base with the Headquarters RuleFlag, or nullptr if none.
    BaseManager* GetHeadquarters();
    const BaseManager* GetHeadquarters() const;

    // Returns buildings the faction has the technology to build.
    std::vector<const BuildingConfig_t*> GetDiscoveredBuildings() const;

    // Sum of constructed copies of buildingId across all bases.
    int CountBuildings(const BuildingId_t& buildingId) const;
    // First base that holds at least one copy, or nullptr.
    BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId);
    const BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId) const;
    // Config of any owned copy, or nullptr when the faction has none.
    const BuildingConfig_t* FindOwnedBuildingConfig(const BuildingId_t& buildingId) const;
    // Copies not on deploy cooldown at missionYear (see DeployBuilding).
    int CountReadyBuildings(const BuildingId_t& buildingId, int missionYear) const;
    // Mark one copy of buildingId deployed until readyMissionYear (inclusive unavailable before).
    void DeployBuilding(const BuildingId_t& buildingId, int readyMissionYear);
    // Drop one deploy record for buildingId when a copy is destroyed (prefer cooling copies).
    void NotifyBuildingDestroyed(const BuildingId_t& buildingId);

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

    // Resource production — routes to all bases. Each base resolves against the composed
    // provider pool (local + world/council via GetActiveEffects).
    void ProduceBaseResources();

    // Apply growth to all bases, incorporating GrowthRate stat effects.
    void ApplyBaseGrowth();

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

    // Destroy the unit and return a snapshot for reconstruct-under-new-owner.
    std::optional<UnitSnapshot_t> ExtractUnit(UnitId_t unitId);
    // Rebuild a unit from a snapshot (clears home / produced-at; foreign claims are invalid).
    Unit& CreateUnitFromSnapshot(const UnitSnapshot_t& rSnapshot,
                                 UnitPositionIndex& rPositions);
    // Extract from this faction and CreateUnitFromSnapshot on rReceiver.
    // Throws if unitId is missing or rReceiver is this faction.
    void TransferUnitTo(UnitId_t unitId,
                        Faction& rReceiver,
                        UnitPositionIndex& rPositions);

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
    // Session world/council extras for IEffectsProvider composition (GameState binds itself).
    void BindWorldEffects(IWorldEffectsSource& rWorldEffects);
    void RebuildVisibility();

    // Optional session prefs (Engine/GameState owned). Used by RebuildVisibility to apply
    // GameRules.RemoveShroud / DebugOptions.RemoveFog without compiling them in.
    void SetSettings(const GameSettings* pSettings) { m_pSettings = pSettings; }
    const GameSettings* GetSettings() const { return m_pSettings; }

    // Optional session back-pointer (GameState::AddFaction). Required for Instantaneous
    // Infiltration dispatch on production completion; null when the faction is unbound.
    void BindGameState(GameState& rGameState) { m_pGameState = &rGameState; }
    GameState* GetGameState() const { return m_pGameState; }

    // Sticky fog removal from ApplyRemoveFog (Instantaneous project completion). Continuous
    // RuleFlag / debug settings are layered on top in ApplyVisibilityRules.
    void SetFogRemoved(bool bFogRemoved) { m_bFogRemoved = bFogRemoved; }
    bool IsFogRemoved() const { return m_bFogRemoved; }

    // Invoked after AddBase (after visibility rebuild). GameState uses this to rebuild
    // world territory; tests may leave it unset and call TerritoryMap::Rebuild directly.
    void SetOnBaseListChanged(std::function<void()> handler);

    // Invoked at the end of RebuildVisibility. GameState uses this for first-contact scans.
    void SetOnVisibilityRebuilt(std::function<void(Faction&)> handler);

    // Pop types
    std::vector<const PopTypeConfig_t*> GetAvailablePopTypes() const;

    // IEffectsProvider — composed view: local FactionEffectsPool plus bound world extras.
    const FactionEffects_t& GetActiveEffects() const override;
    uint64_t GetEffectsVersion() const override;

    // Local pool only (no peer WorldGlobal / council). Used when harvesting peers for
    // world composition, and for social-rating accumulation / faction-lane rating expand
    // (ratings are a faction-internal axis — see SocialRatingResolver).
    const FactionEffects_t& GetLocalActiveEffects() const override;
    uint64_t GetLocalEffectsVersion() const;

private:
    void EnsureComposedEffects_() const;
    int GetResearchPerTurn_() const;

    FactionId_t m_factionId;
    bool m_bIsPlayerControlled;
    const FactionConfig_t& m_rDefinition;
    const GameDataContext& m_rDataContext;
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
    IWorldEffectsSource* m_pWorldEffects = nullptr; // set by BindWorldEffects; optional
    const GameSettings* m_pSettings = nullptr; // non-owning; optional session prefs
    GameState* m_pGameState = nullptr; // set by BindGameState; optional session back-pointer
    bool m_bFogRemoved = false; // sticky ApplyRemoveFog
    std::function<void()> m_onBaseListChanged;
    std::function<void(Faction&)> m_onVisibilityRebuilt;

    // Composed GetActiveEffects cache (local pool + world extras). The two stamps are the
    // cache key; m_composedVersion is what GetEffectsVersion publishes.
    mutable FactionEffects_t m_composedEffects;
    mutable uint64_t m_composedLocalVersion = UINT64_MAX;
    mutable uint64_t m_composedWorldStamp = UINT64_MAX;
    mutable uint64_t m_composedVersion = 0;

    // Shared ASAT / intercept deploy cooldowns keyed by building id (no per-instance ids).
    // A copy is unavailable while missionYear < readyMissionYear.
    struct BuildingDeploy_t
    {
        BuildingId_t buildingId;
        int readyMissionYear = 0;
    };
    std::vector<BuildingDeploy_t> m_buildingDeploys;
};

} // namespace ac
