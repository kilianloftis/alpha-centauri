#pragma once

#include <memory>
#include <optional>
#include <functional>
#include <ranges>
#include <vector>

#include "game/IEffectsProvider.h"
#include "game/IWorldEffectsSource.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingUpkeep.h"
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
#include "lib/Signal.h"
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

// Deref the owning pointers, then drop razed bases. Free rather than a member because both
// Bases() overloads deduce their return type from it, and an in-class helper is not deduced
// early enough for that. See Faction::Bases.
template <typename Vec>
auto LiveBasesView(Vec& rBases)
{
    return DerefView(rBases)
        | std::views::filter([](const BaseManager& rBase) { return !rBase.IsRazed(); });
}

class Faction : public IEffectsProvider
{
public:
    // rDataContext is the game-wide definition data (registries plus the calculators built
    // from them). It is held by reference for the faction's whole life and handed to the
    // subsystems that need pieces of it, so adding a new kind of shared game data does not
    // change this signature. Must outlive the faction — see Engine's member ordering.
    //
    // rWorldMap and rSettings are constructor dependencies rather than post-construction
    // binds: a faction without a map used to early-return out of RebuildVisibility, so a base
    // founded on a not-yet-wired faction silently produced no visibility, no territory rebuild
    // and no first-contact check while every getter still returned plausible values. Taking
    // them here makes that state unrepresentable. The constructor sizes the explored/visible
    // maps from rWorldMap. The remaining session wiring (GameState back-pointer and the two
    // observers) is applied by GameState::AddFaction — see AttachToSession.
    //
    // seed drives every per-faction random choice (base names, the starting research target).
    // It is injected rather than drawn from std::random_device inside the sub-objects, because
    // those choices are save-game state: an unseedable generator meant the same session could
    // not be reproduced. The composition root derives it from the session seed.
    Faction(FactionId_t factionId,
             bool bIsPlayerControlled,
             const FactionConfig_t& rDefinition,
             const GameDataContext& rDataContext,
             WorldMap& rWorldMap,
             const GameSettings& rSettings,
             uint32_t seed);
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
    // Destroy the base (razed on capture) and return a snapshot of its
    // pre-destruction state. This is the DESTROY path: home-base claims orphan, the object
    // is gone, and CreateBaseFromSnapshot is a *reconstruction* (new address), not a live
    // transfer. See TransferBaseTo for ownership change that preserves identity.
    std::optional<BaseSnapshot_t> ExtractBase(BaseId_t baseId);
    // Move a base out of this faction's ownership without destroying it (identity-preserving
    // transfer's building block). Throws if baseId is missing.
    std::unique_ptr<BaseManager> ReleaseBase(BaseId_t baseId);
    // Rebuild a base from a snapshot (recalculates pop roles and worked tiles). New object,
    // new address — for future save/load reconstruction, NOT the live transfer path (see
    // TransferBaseTo).
    BaseManager* CreateBaseFromSnapshot(const BaseSnapshot_t& rSnapshot,
                                        const GameDataContext& rDataContext,
                                        TileEffectsContext& rTileEffects,
                                        const SecretProjectAvailabilityCalculator& rSecretProjectAvailability);
    // Identity-preserving ownership move: ReleaseBase → rebind owner → rReceiver.AddBase.
    // Same BaseManager object and baseId end up owned by rReceiver; no destroy/recreate, no
    // fake events. Recalculates composition and worker assignment under the new owner
    // (psych may differ) and migrates this base's building deploy cooldowns.
    // Throws if baseId is missing or rReceiver is this faction.
    void TransferBaseTo(BaseId_t baseId, Faction& rReceiver);
    // Factory method: unpacks the individual registries/calculators BaseManager needs from
    // rDataContext (a composition-root-supplied bag) so BaseManager itself can declare narrow,
    // named dependencies instead of taking the whole context. If the faction has no
    // Headquarters, the new base is granted that building (first-base / rebuilt-HQ founding).
    BaseManager* CreateBase(BaseId_t baseId, const std::string& name, Tile* pTile,
                            const GameDataContext& rDataContext,
                            TileEffectsContext& rTileEffects,
                            const SecretProjectAvailabilityCalculator& rSecretProjectAvailability);
    // Iterate live bases by reference without exposing the owning unique_ptrs. Razed bases are
    // filtered out here rather than erased at raze time, which is what lets a base die from
    // inside a signal handler or mid-iteration: every existing loop stops seeing it at once,
    // while the object itself survives until ReapRazedBases.
    auto Bases() { return LiveBasesView(m_bases); }
    auto Bases() const { return LiveBasesView(m_bases); }
    size_t GetBaseCount() const;

    // Live base with this id, or nullptr. A razed base is already gone as far as this answers.
    BaseManager* FindBase(BaseId_t baseId);
    const BaseManager* FindBase(BaseId_t baseId) const;

    // Take rBase out of the game now: mark secret projects destroyed, orphan home-base claims,
    // release the tile, drop deploy records, and emit BaseManager::OnDestroyed. Everything the
    // rest of the game can observe happens here; only the object's destruction waits for
    // ReapRazedBases. Idempotent — the pop-loss handler and a conquest path may both ask.
    // Throws if rBase is not this faction's.
    void RazeBase(BaseManager& rBase);

    // Destroy the BaseManagers razed since the last call. Pure memory hygiene: a razed base is
    // already invisible to Bases() / FindBase, so the timing carries no game meaning. Called
    // between turn stages (TurnProcessor::Advance), where nothing is iterating bases.
    void ReapRazedBases();

    // Fired at the end of AddBase for every insertion — founding, load, and post-transfer
    // adopt alike. The single hook EventBridge wires from (see EventBridge::WireBase);
    // conquest/probe/diplomacy callers do not need to remember to wire anything themselves.
    Signal<BaseManager&> OnBaseAdded;

    // Sum of population size across all bases.
    int TotalPopulation() const;

    // Base with the Headquarters RuleFlag, or nullptr if none.
    BaseManager* GetHeadquarters();
    const BaseManager* GetHeadquarters() const;


    // Sum of constructed copies of buildingId across all bases.
    int CountBuildings(const BuildingId_t& buildingId) const;
    // First base that holds at least one copy, or nullptr.
    BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId);
    const BaseManager* FindBaseWithBuilding(const BuildingId_t& buildingId) const;
    // Config of any owned copy, or nullptr when the faction has none.
    const BuildingConfig_t* FindOwnedBuildingConfig(const BuildingId_t& buildingId) const;
    // Copies not on deploy cooldown at missionYear (see DeployBuilding). A pure query: asking
    // about a hypothetical or future year must not mutate the ledger, so the answer does not
    // depend on what was asked before. Expiry is retired by PruneExpiredDeploys.
    int CountReadyBuildings(const BuildingId_t& buildingId, int missionYear) const;
    // Retire every deploy record that has become ready at missionYear, so the ledger cannot
    // grow unbounded across many deploy cycles. Called once per faction per turn (Upkeep
    // stage); dropping a record is equivalent to it counting as ready, so this changes no
    // CountReadyBuildings answer for the current or any later year.
    void PruneExpiredDeploys(int missionYear);
    // Mark one copy of buildingId at baseId deployed until readyMissionYear (inclusive
    // unavailable before). Keyed by base (best-effort: the caller's base attribution, not a
    // tracked per-instance id) so the record cannot outlive that base's ownership of the
    // copy: ReleaseBase/ExtractBase migrate or drop matching records with the base.
    void DeployBuilding(BaseId_t baseId, const BuildingId_t& buildingId, int readyMissionYear);
    // Drop the deploy record for buildingId at baseId when that specific copy is destroyed.
    // No-op if the copy at baseId was not cooling — never drops another base's record for
    // the same buildingId.
    void NotifyBuildingDestroyed(BaseId_t baseId, const BuildingId_t& buildingId);

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
    std::optional<int> GetTurnsUntilBreakthrough() const;

    // Resource production — routes to all bases. Each base resolves against the composed
    // provider pool (local + world/council via GetActiveEffects), applies inefficiency,
    // splits energy into econ/labs/psych locally, then stockpiles. Faction collection of
    // econ/labs is CollectIncome / CollectResearch; psych stays at the base.
    void ProduceBaseResources();

    // Charge mineral support at every base (home units vs this turn's mineral bank).
    // Called from the UnitSupport stage, after ResourceCollection has banked minerals and
    // before MineralConversion claims what is left.
    void ApplyMineralSupport();

    // Claim leftover minerals at every base (bank into a real build, convert a stockpile,
    // or waste). Called from the MineralConversion stage, which turn_stages.json orders
    // after UnitSupport and before IncomeCollection / ResearchAccumulation so stockpile
    // income / research see the credits this turn.
    void ConvertMinerals();

    // Sum of BuildingConfig_t::upkeep across every constructed building copy (all bases).
    // Continuous GrantBuilding expansions are not constructed and are not included.
    int GetBuildingUpkeep() const;

    // Constructed buildings across all bases, grouped by type for UI (name, count, upkeep).
    std::vector<BuildingUpkeepLine_t> GetBuildingUpkeepByType() const;

    // Deduct GetBuildingUpkeep() from the faction energy treasury.
    // Called from Upkeep after IncomeCollection has banked this turn's econ.
    // Throws if the treasury cannot cover the cost (no auto-sell yet).
    void ApplyBuildingUpkeep();

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

    // Identity-preserving ownership move (subvert / trade): release from this faction's
    // UnitManager and adopt into rReceiver's, without DestroyUnit's combat cargo-loss rules
    // and without emitting OnUnitDestroyed/OnUnitCreated (see UnitManager::ReleaseUnit /
    // AdoptUnit and OnUnitReleased / OnUnitAdopted). A transferred carrier's cargo travels
    // with it; a transferred embarked passenger detaches from its carrier cleanly (no throw,
    // no CanPlaceUnitOnTile re-check — the unit never leaves the map). Home base is cleared
    // (a foreign home claim is invalid). Throws if unitId is missing or rReceiver is this
    // faction.
    void TransferUnitTo(UnitId_t unitId, Faction& rReceiver);

    // Amount and sink if one constructed copy of this type were scrapped at rBase now.
    // Empty when the id is not held there, the copy is a secret project, the kind has no
    // default_scrap, or the item override cleared the formula. Base-destined refunds land at
    // rBase. Throws if rBase is not this faction's.
    std::optional<ScrapPayout_t> QuoteScrapBuilding(const BaseManager& rBase,
                                                   const BuildingId_t& buildingId) const;

    // Player scrap: destroy one constructed copy at rBase and credit QuoteScrapBuilding.
    // Combat / probe / raze / orbital destruction must not call this. Throws if rBase is
    // not this faction's, or QuoteScrapBuilding is empty. Returns the amount granted.
    int ScrapBuilding(BaseManager& rBase, const BuildingId_t& buildingId);

    // Same quote over every constructed copy of this type across this faction's bases.
    // Empty when no base holds a copy QuoteScrapBuilding would accept. Amount is the sum of
    // those per-copy quotes. destBaseId is set only when every copy pays into the same sink
    // (none for the treasury).
    std::optional<ScrapPayout_t> QuoteScrapBuildings(const BuildingId_t& buildingId) const;

    // Player scrap: destroy every constructed copy of that type at every base, each through
    // ScrapBuilding. Same combat / probe / raze / orbital restriction. Throws if
    // QuoteScrapBuildings is empty. Returns the amount granted.
    int ScrapBuildings(const BuildingId_t& buildingId);

    // Amount and sink if this unit were scrapped now. Empty when kinds.unit has no
    // default_scrap. A base-destined refund goes to the closest friendly base, and pays 0
    // when the unit is off this faction's territory — the unit can still be scrapped.
    // Throws if the unit is not this faction's.
    std::optional<ScrapPayout_t> QuoteScrapUnit(const Unit& rUnit) const;

    // Player scrap: destroy the unit and credit QuoteScrapUnit. Combat, support disband, and
    // other DestroyUnit paths do not refund. Throws if QuoteScrapUnit is empty. Returns the
    // amount granted.
    int ScrapUnit(Unit& rUnit);

    // Fog of war: permanent explored memory and currently-visible tiles as separate maps.
    // The constructor sizes both from the world map and takes a first reading;
    // RebuildVisibility refreshes current vision from units/bases (and grows explored). Unit
    // create/destroy call it from UnitManager; moves reach it via
    // UnitPositionIndex::OnUnitMoved (wired by GameState); base founding invokes it from
    // AddBase.
    FactionExploredMap& GetExploredMap();
    const FactionExploredMap& GetExploredMap() const;
    FactionVisibleMap& GetVisibleMap();
    const FactionVisibleMap& GetVisibleMap() const;
    // Contact reveal: concealed units this faction has bumped into (occupied tile / ZOC).
    FactionRevealedUnits& GetRevealedUnits();
    const FactionRevealedUnits& GetRevealedUnits() const;
    const WorldMap& GetWorldMap() const { return m_rWorldMap; }
    // Session world/council extras for IEffectsProvider composition (GameState binds itself).
    void BindWorldEffects(IWorldEffectsSource& rWorldEffects);
    void RebuildVisibility();

    // Coalesces visibility rebuilds. While any scope is open, RebuildVisibility only marks the
    // map dirty; the outermost scope performs one rebuild on destruction. A rebuild re-reveals
    // from every unit and base, walks every tile for vision improvements, and then drives a
    // first-contact sweep over every other faction — so a burst of unit events (sinking a loaded
    // transport destroys the carrier and each passenger) must not pay for it once per event.
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

    // Read by FactionEffectsPool to re-resolve the session difficulty on rebuild.
    const GameDataContext& GetDataContext() const { return m_rDataContext; }

    // Optional session back-pointer (GameState::AttachToSession). Required for Instantaneous
    // Infiltration dispatch on production completion; null when the faction is unbound.
    void BindGameState(GameState& rGameState) { m_pGameState = &rGameState; }
    // Non-const: the session is handed out to be mutated (Instantaneous Infiltration writes
    // the diplomacy ledger), so a const Faction must not be able to produce it.
    GameState* GetGameState() { return m_pGameState; }

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
    WorldMap& m_rWorldMap; // constructor-injected; RebuildVisibility can never be a no-op
    const GameSettings& m_rSettings; // non-owning session prefs; constructor-injected
    IWorldEffectsSource* m_pWorldEffects = nullptr; // set by BindWorldEffects; optional
    GameState* m_pGameState = nullptr; // set by BindGameState; optional session back-pointer
    bool m_bFogRemoved = false; // sticky ApplyRemoveFog
    int m_visibilityDeferralDepth = 0;
    bool m_bVisibilityDirty = false;
    std::function<void()> m_onBaseListChanged;
    std::function<void(Faction&)> m_onVisibilityRebuilt;

    // Composed GetActiveEffects cache (local pool + world extras). The two stamps are the
    // cache key; m_composedVersion is what GetEffectsVersion publishes.
    mutable FactionEffects_t m_composedEffects;
    mutable uint64_t m_composedLocalVersion = UINT64_MAX;
    mutable uint64_t m_composedWorldStamp = UINT64_MAX;
    mutable uint64_t m_composedVersion = 0;

    // Migrate every deploy record for baseId to rReceiver (TransferBaseTo) so a cooling
    // ASAT/interceptor copy keeps its cooldown under the new owner instead of leaking a
    // phantom suppression in the old one. Extract/raze simply drops them (see ExtractBase).
    void MigrateBuildingDeploys_(BaseId_t baseId, Faction& rReceiver);
    // Drop every deploy record for baseId (base destroyed: razed on capture).
    void DropBuildingDeploys_(BaseId_t baseId);

    // Base that receives a unit's base-destined scrap refund on rTile: the closest live base
    // by TabletopDiagonalDistance (horizontal wrap), then lower BaseId. Empty when rTile is
    // outside this faction's territory or the faction holds no bases.
    // TODO: SMAC's nearest-base metric for unit disband is not recorded.
    std::optional<BaseId_t> ScrapDestinationFor_(const Tile& rTile) const;

    // ASAT / intercept deploy cooldowns, keyed by (baseId, buildingId) — the base attribution
    // is the calling site's best-effort pick of "a base holding a copy," not a tracked
    // per-instance id (see DeployBuilding). A copy is unavailable while
    // missionYear < readyMissionYear. Expired records are retired by PruneExpiredDeploys,
    // not by the CountReadyBuildings query, which is const in both senses.
    struct BuildingDeploy_t
    {
        BaseId_t baseId = 0;
        BuildingId_t buildingId;
        int readyMissionYear = 0;
    };
    std::vector<BuildingDeploy_t> m_buildingDeploys;
};

} // namespace ac
