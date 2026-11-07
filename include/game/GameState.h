#pragma once

#include "game/faction/Faction.h"
#include "game/effects/IWorldEffectsSource.h"
#include "game/units/IUnitOrderWorld.h"
#include "lib/DerefView.h"
#include "lib/IdAllocator.h"
#include "lib/Signal.h"
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class TileEffectsContext;
class EventBus;
class WorldMap;
class PlayerInteractionQueue;
class DiplomacyLedger;
class DiplomaticActionExecutor;
class FirstContactResolver;
class AtrocityLedger;
class MindControlLedger;
class EcologyLedger;
class WorldEventTracker;
class MoveCostRules;
class StepEvaluator;
class Pathfinder;
class UnitMoveExecutor;
class UnitOrderExecutor;
class UnitAttackExecutor;
class ProbeActionExecutor;
class GameSettings;
struct GameDataContext;
class CouncilVoteExecutor;
class PlanetaryCouncil;

class GameState : public IUnitOrderWorld, public IWorldEffectsSource
{
public:
    // One less than the first playable year: TurnStart increments at the start of every turn,
    // so the first Advance lands on k_FirstPlayableMissionYear.
    static constexpr int k_StartingMissionYear = 2099;
    static constexpr int k_FirstPlayableMissionYear = k_StartingMissionYear + 1;

    // Throws if pWorldMap is null. rGameData must outlive this. rngSeed makes the session's rolls
    // reproducible.
    GameState(std::unique_ptr<WorldMap> pWorldMap,
              const GameDataContext& rGameData,
              GameSettings& rSettings,
              uint32_t rngSeed);
    ~GameState();

    // Builds the council and its vote executor from the participating factions, once the
    // starting factions are in. Throws if already created.

    // Registries, configs and calculators: the rules every stage, rule module and view reads.
    const GameDataContext& GetGameData() const;

    int GetMissionYear() const;
    void IncrementMissionYear();
    // Years since k_FirstPlayableMissionYear (0 on the first playable year); never negative.
    int GetYearsSinceFirstPlayableYear() const;
    // terranx CurrentTurn: 0 during setup, 1 on the first playable year.
    int GetTurnNumber() const;

    // Shared combat / probe / world-event roll stream for this session.
    std::mt19937& GetRng();
    const std::mt19937& GetRng() const;

    GameSettings& GetSettings();
    const GameSettings& GetSettings() const;

    EventBus& GetEventBus();
    const EventBus& GetEventBus() const;

    // Mid-turn player notices / choices / open-view requests. Stages Enqueue and Yield;
    // InteractionPresenter presents Front and CompleteFront after resolution.
    PlayerInteractionQueue& GetPlayerInteractions();
    const PlayerInteractionQueue& GetPlayerInteractions() const;

    // Keys of triggered effects carrying oncePer scope World (first-discover bonuses, …).
    std::set<std::string>& ConsumedTriggerKeys() { return m_consumedTriggerKeys; }
    const std::set<std::string>& ConsumedTriggerKeys() const { return m_consumedTriggerKeys; }

    // Registers an already valid faction and wires it into the session, even one that owns bases.
    // Throws when it was built against another WorldMap.
    Faction& AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;
    // Iterate factions by reference without exposing the owning unique_ptrs.
    auto Factions() { return DerefView(m_factions); }
    auto Factions() const { return DerefView(m_factions); }
    // nullptr when no faction with that id has been added.
    Faction* FindFaction(FactionId_t factionId);
    const Faction* FindFaction(FactionId_t factionId) const;
    // Throws std::invalid_argument when no faction with that id has been added.
    Faction& RequireFaction(FactionId_t factionId);
    const Faction& RequireFaction(FactionId_t factionId) const;
    // Returns the faction with IsPlayerControlled() set, or nullptr if none has been added yet.
    // Throws when no faction is player-controlled; Engine always seats one.
    const Faction& GetPlayerFaction() const;
    Faction& GetPlayerFaction();

    DiplomacyLedger& GetDiplomacyLedger();
    const DiplomacyLedger& GetDiplomacyLedger() const;

    // Committed atrocities and the commerce sanctions they carry.
    AtrocityLedger& GetAtrocityLedger();
    const AtrocityLedger& GetAtrocityLedger() const;

    // Mind control each faction has done; the base mind-control quote reads the actor's total.
    MindControlLedger& GetMindControlLedger();
    const MindControlLedger& GetMindControlLedger() const;

    // Fungal blooms, clean-mineral grants and virtual minerals each faction carries.
    EcologyLedger& GetEcologyLedger();
    const EcologyLedger& GetEcologyLedger() const;

    DiplomaticActionExecutor& GetDiplomaticActionExecutor();

    // The only source of faction, base and unit ids.
    FactionId_t AllocateFactionId();
    BaseId_t AllocateBaseId();
    UnitId_t AllocateUnitId();
    IdAllocator& GetUnitIdAllocator() { return m_unitIdAllocator; }

    WorldMap& GetWorldMap();
    const WorldMap& GetWorldMap() const;

    // Base whose center tile is (tileX, tileY), or nullptr if none.
    BaseManager* FindBaseAt(int tileX, int tileY) override;
    const BaseManager* FindBaseAt(int tileX, int tileY) const;
    // Any faction's base closest to rTile by TabletopDiagonalDistance (the binary's base_find
    // and vector_dist), then the lower BaseId. nullptr when no base exists.
    const BaseManager* FindNearestBase(const Tile& rTile) const;

    std::optional<CombatResult_t> TryInterceptAttack(
        Unit& rAttacker, Unit& rDefender, std::mt19937& rRng) override;
    BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover, std::mt19937& rRng) override;
    void OnHostileAct(const HostileAct_t& rAct) override;
    void ApplyFightConsequences(Unit& rAttacker, const Tile& rBattleTile,
                                CombatResult_t& rResult) override;
    void OnImprovementVisited(Unit& rVisitor) override;
    void OnTerritoryEntryRefused(Unit& rUnit, FactionId_t territoryOwner) override;
    bool ApplyTerraformCompleteEffects(Tile& rTile, const TerraformProject_t& rProject,
                                       Unit& rFormer, std::mt19937& rRng) override;

    // Peer WorldGlobal effects and council extras, bound onto each faction's pool.
    std::vector<ActiveEffect_t> CollectWorldExtras(const Faction& rFor) const override;
    uint64_t GetWorldCompositionStamp(const Faction& rFor) const override;
    std::vector<ActiveEffect_t> CollectSessionWorldEffects() const override;

    // Tile effects context (WorldMap + ImprovementRegistry bundled for tile resolution).
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    // Shared path search for order execution and UI preview.
    Pathfinder& GetPathfinder();
    const Pathfinder& GetPathfinder() const;

    UnitMoveExecutor& GetUnitMoves();
    UnitOrderExecutor& GetUnitOrderExecutor();
    UnitAttackExecutor& GetUnitAttacks();

    // A Hold whose design matches this friendly base: the player is asked to link, an AI links at
    // once. Also runs when a building completes under a Holding unit.
    void ConsiderHoldLink(Unit& rUnit);

    ProbeActionExecutor& GetProbeActions();
    const ProbeActionExecutor& GetProbeActions() const;


    PlanetaryCouncil& GetPlanetaryCouncil();
    const PlanetaryCouncil& GetPlanetaryCouncil() const;
    CouncilVoteExecutor& GetCouncilVotes();

    // Start tracking the world-event registry. Throws if already created. Without it the
    // WorldEvents stage runs no events.
    void CreateWorldEvents();
    WorldEventTracker* GetWorldEvents();
    const WorldEventTracker* GetWorldEvents() const;

    // Recompute world territory from every faction's bases. Also wired as each faction's
    // OnBaseListChanged handler so founding a base keeps ownership current.
    void RebuildTerritory();

    // Secret Projects lost when a base is razed (pop → 0). Tombstoned so they can never be
    // rebuilt by any faction.
    void MarkSecretProjectDestroyed(const std::string& buildingId);

    bool IsSecretProjectDestroyed(const std::string& buildingId) const;

    // Frees bases razed since the last call, between stages where nothing iterates bases.
    void ReapRazedBases();

    // Public orbital building census (buildings with orbital == true). Visible to all factions.



private:
    void OnVisibilitySettingsChanged_();
    // One source list for both collectors, so they cannot drift.
    void AppendWorldSources_(const Faction* pFor, std::vector<ActiveEffect_t>& rOut) const;
    // Tombstoned SPs contribute authored destroyed_effects for the session.
    void AppendDestroyedSecretProjectEffects_(std::vector<ActiveEffect_t>& rOut) const;
    // Applied after the faction is in m_factions, since the observers iterate Factions(). Ends
    // with a territory and visibility sweep.
    void AttachToSession_(Faction& rFaction);
    void WireFactionObservers_(Faction& rFaction);
    void WireUnitObservers_(UnitManager& rUnits);
    void WireBaseObservers_(Faction& rFaction);
    void WireBaseSession_(BaseManager& rBase);
    void ConsiderAllObservers_();
    void OnUnitMoved_(Unit& rMoved);

    int m_missionYear;
    GameSettings& m_rSettings;
    const GameDataContext& m_rGameData;
    Signal<>::ScopedConnection m_visibilitySettingsChanged;
    std::unique_ptr<EventBus> m_pEventBus;
    std::unique_ptr<PlayerInteractionQueue> m_pPlayerInteractions;
    std::set<std::string> m_consumedTriggerKeys;
    // Before m_factions, so they outlive the BaseManagers that reference TileEffectsContext.
    std::unique_ptr<WorldMap> m_worldMap;
    std::unique_ptr<TileEffectsContext> m_pTileEffects;
    std::unique_ptr<MoveCostRules> m_pMoveCosts;
    std::unique_ptr<StepEvaluator> m_pSteps;
    std::unique_ptr<Pathfinder> m_pPathfinder;
    std::unique_ptr<DiplomacyLedger> m_pDiplomacy;
    std::unique_ptr<AtrocityLedger> m_pAtrocities;
    std::unique_ptr<MindControlLedger> m_pMindControl;
    std::unique_ptr<EcologyLedger> m_pEcology;
    std::unique_ptr<DiplomaticActionExecutor> m_pDiplomaticActionExecutor;
    std::vector<std::unique_ptr<Faction>> m_factions;
    std::unique_ptr<FirstContactResolver> m_pFirstContact;
    // Before the executors that reference it.
    std::mt19937 m_rng;
    std::unique_ptr<UnitMoveExecutor> m_pUnitMoves;
    std::unique_ptr<UnitOrderExecutor> m_pUnitOrderExecutor;
    std::unique_ptr<UnitAttackExecutor> m_pUnitAttacks;
    std::unique_ptr<ProbeActionExecutor> m_pProbeActions;
    std::unique_ptr<PlanetaryCouncil> m_pCouncil;
    // Declared after the council it holds a reference to.
    std::unique_ptr<CouncilVoteExecutor> m_pCouncilVotes;
    std::unique_ptr<WorldEventTracker> m_pWorldEvents;
    IdAllocator m_factionIdAllocator;
    IdAllocator m_baseIdAllocator;
    IdAllocator m_unitIdAllocator;
    std::unordered_set<std::string> m_destroyedSecretProjects;
    // Idempotent session wiring for production on_complete + mood notices (transfer-safe).
    std::unordered_set<BaseManager*> m_sessionWiredBases;
};

} // namespace ac
