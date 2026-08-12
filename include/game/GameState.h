#pragma once

#include "game/Faction.h"
#include "game/IWorldEffectsSource.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/FirstContactResolver.h"
#include "game/map/WorldMap.h"
#include "game/orbital/OrbitalAttack.h"
#include "game/orbital/OrbitalCensus.h"
#include "game/PlayerInteractionQueue.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Pathfinder.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/ProbeActionExecutor.h"
#include "lib/DerefView.h"
#include "lib/IdAllocator.h"
#include "lib/Signal.h"
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TileEffectsContext;
class UnitComponentRegistry;
class EventBus;
class GameSettings;
class CouncilProposalRegistry;
struct CouncilRulesConfig_t;
class PlanetaryCouncil;

class GameState : public IUnitOrderWorld, public IWorldEffectsSource
{
public:
    // One less than the first playable year: TurnStart increments at the start of every turn,
    // so the first Advance lands on k_FirstPlayableMissionYear.
    static constexpr int k_StartingMissionYear = 2099;
    static constexpr int k_FirstPlayableMissionYear = k_StartingMissionYear + 1;

    // pUnitComponents sizes the aura scan for unit-projected ThisTile effects; may be null
    // if units never project auras. Throws if pWorldMap is null.
    // rSettings is a non-owning reference to Engine-owned player preferences (not save state).
    // rMorale is the GameDataContext-owned calculator (XP ranks / combat % / promotion),
    // borrowed here for the combat and probe paths. It must outlive this GameState.
    // rngSeed seeds the session RNG behind every combat, promotion and probe roll. Injected
    // rather than drawn from std::random_device so a session is reproducible from the seed the
    // composition root resolves and reports; tests pass a fixed value to keep rolls stable.
    GameState(std::unique_ptr<WorldMap> pWorldMap,
              const ImprovementRegistry& rImprovements,
              const UnitComponentRegistry* pUnitComponents,
              GameSettings& rSettings,
              const MoraleCalculator& rMorale,
              uint32_t rngSeed);
    ~GameState();

    // Build the Planetary Council from factions currently in this GameState that
    // participate in the council. Call after the starting factions have been added.
    // Registry/rules must outlive this GameState. Throws if already created.
    void CreatePlanetaryCouncil(const CouncilProposalRegistry& rRegistry,
                                const CouncilRulesConfig_t& rRules);

    const MoraleCalculator& GetMoraleCalculator() const;

    // Mission year
    int GetMissionYear() const;
    void SetMissionYear(int year);
    void IncrementMissionYear();
    // Years since k_FirstPlayableMissionYear (0 on the first playable year); never negative.
    int GetYearsSinceFirstPlayableYear() const;

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

    // Factions. Registration only: the faction is already valid when it arrives (world map and
    // settings are constructor dependencies). This adds the session wiring that must close over
    // GameState — see AttachToSession_ — and works for a faction that already owns bases/units.
    Faction& AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;
    // Iterate factions by reference without exposing the owning unique_ptrs.
    auto Factions() { return DerefView(m_factions); }
    auto Factions() const { return DerefView(m_factions); }
    // nullptr when no faction with that id has been added.
    Faction* FindFaction(FactionId_t factionId);
    const Faction* FindFaction(FactionId_t factionId) const;
    // Returns the faction with IsPlayerControlled() set, or nullptr if none has been added yet.
    const Faction* GetPlayerFaction() const;
    Faction* GetPlayerFaction();

    DiplomacyLedger& GetDiplomacyLedger();
    const DiplomacyLedger& GetDiplomacyLedger() const;

    DiplomaticActionExecutor& GetDiplomaticActionExecutor();

    // Sole owners of faction/base/unit ID allocation: nothing else may mint one of these
    // IDs, so any runtime faction, base, or unit creation (not just the composition root)
    // has a place to get a unique ID from.
    FactionId_t AllocateFactionId();
    BaseId_t AllocateBaseId();
    UnitId_t AllocateUnitId();

    // World map
    WorldMap& GetWorldMap();
    const WorldMap& GetWorldMap() const;

    // Base whose center tile is (tileX, tileY), or nullptr if none.
    BaseManager* FindBaseAt(int tileX, int tileY) override;
    const BaseManager* FindBaseAt(int tileX, int tileY) const;

    std::optional<CombatResult_t> TryInterceptAttack(
        Unit& rAttacker, Unit& rDefender, TileEffectsContext& rTileEffects,
        std::mt19937& rRng) override;
    BaseConquestResult_t ResolvePostCombatBaseConquest(
        Unit& rAttacker, const Tile& rDefenderTile, const GameDataContext& rDataContext,
        std::mt19937& rRng) override;
    BaseConquestResult_t ResolveBaseEntryConquest(
        Unit& rMover, const GameDataContext& rDataContext, std::mt19937& rRng) override;

    // IWorldEffectsSource — peer WorldGlobal (from local pools) + council extras for rFor.
    // Bound onto each Faction in AddFaction so GetActiveEffects composes one pool.
    std::vector<ActiveEffect_t> CollectWorldExtras(const Faction& rFor) const override;
    uint64_t GetWorldCompositionStamp(const Faction& rFor) const override;

    // Tile effects context (WorldMap + ImprovementRegistry bundled for tile resolution).
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    // Shared path search for order execution and UI preview.
    Pathfinder& GetPathfinder();
    const Pathfinder& GetPathfinder() const;

    UnitOrderExecutor& GetUnitOrderExecutor();

    ProbeActionExecutor& GetProbeActions();
    const ProbeActionExecutor& GetProbeActions() const;

    FirstContactResolver& GetFirstContactResolver();
    const FirstContactResolver& GetFirstContactResolver() const;

    // Null until CreatePlanetaryCouncil (most unit-test fixtures never create one).
    PlanetaryCouncil* GetPlanetaryCouncil();
    const PlanetaryCouncil* GetPlanetaryCouncil() const;

    // Recompute world territory from every faction's bases. Also wired as each faction's
    // OnBaseListChanged handler so founding a base keeps ownership current.
    void RebuildTerritory();

    // Scans all bases of all factions to check whether a secret project has already been
    // built. Owned here (rather than GameDataContext) because it queries this live,
    // mutable faction data — an "immutable definition data" object referencing it would be
    // constructible only after GameState exists, and would dangle if GameState were ever
    // rebuilt (new game / load game).
    const SecretProjectAvailabilityCalculator& GetSecretProjectAvailability() const;

    // Secret Projects lost when a base is razed (pop → 0). Tombstoned so they can never be
    // rebuilt by any faction.
    void MarkSecretProjectDestroyed(const std::string& buildingId);

    // The one raze pathway. Tombstones any secret project the base held (nobody may rebuild it)
    // and removes the base from its owner. Every way a base can be destroyed — conquest,
    // starving to nothing — goes through here, so the tombstoning and the removal cannot drift
    // apart. Not safe to call from inside one of the base's own signal handlers: it destroys the
    // BaseManager.
    void RazeBase(BaseManager& rBase);
    bool IsSecretProjectDestroyed(const std::string& buildingId) const;

    // Public orbital building census (buildings with orbital == true). Visible to all factions.
    std::vector<OrbitalCensusEntry_t> GetOrbitalCensus() const;
    int CountOrbitalBuildings(FactionId_t factionId, const BuildingId_t& buildingId) const;

    // Ready OrbitalAttack buildings for rFaction (UI selection list).
    std::vector<OrbitalAttackerOption_t> ListReadyOrbitalAttackers(const Faction& rFaction) const;

    // ASAT using a chosen ready attacker building against another faction's orbital.
    // Uses GameState RNG.
    OrbitalAttackResult_t TryAttackSatellite(Faction& rAttacker,
                                             Faction& rDefender,
                                             const BuildingId_t& attackerBuildingId,
                                             const BuildingId_t& targetOrbitalBuildingId);

private:
    void OnVisibilitySettingsChanged_();
    // All session wiring for one faction (back-pointer + observers), applied by AddFaction
    // after the faction is in m_factions — the observers iterate Factions(), so the order is
    // load-bearing. Ends with a territory/visibility sweep so a faction that arrives already
    // populated is scanned rather than silently skipped.
    void AttachToSession_(Faction& rFaction);

    int m_missionYear;
    GameSettings& m_rSettings;
    const MoraleCalculator& m_rMorale;
    Signal<>::ScopedConnection m_visibilitySettingsChanged;
    std::unique_ptr<EventBus> m_pEventBus;
    PlayerInteractionQueue m_playerInteractions;
    // WorldMap and TileEffectsContext are declared before m_factions so they outlive all
    // BaseManagers (which hold TileEffectsContext& references). Members are destroyed in
    // reverse declaration order, so m_factions is destroyed before these two.
    std::unique_ptr<WorldMap> m_worldMap;
    std::unique_ptr<TileEffectsContext> m_pTileEffects;
    std::unique_ptr<MoveCostCalculator> m_pMoveCosts;
    std::unique_ptr<StepEvaluator> m_pSteps;
    std::unique_ptr<Pathfinder> m_pPathfinder;
    std::unique_ptr<DiplomacyLedger> m_pDiplomacy;
    std::unique_ptr<DiplomaticActionExecutor> m_pDiplomaticActionExecutor;
    std::vector<std::unique_ptr<Faction>> m_factions;
    std::unique_ptr<FirstContactResolver> m_pFirstContact;
    // Shared combat / promotion / probe roll stream. Declared before the executors that
    // hold references into it.
    std::mt19937 m_rng;
    std::unique_ptr<UnitOrderExecutor> m_pUnitOrderExecutor;
    std::unique_ptr<ProbeActionExecutor> m_pProbeActions;
    std::unique_ptr<PlanetaryCouncil> m_pCouncil;
    IdAllocator m_factionIdAllocator;
    IdAllocator m_baseIdAllocator;
    IdAllocator m_unitIdAllocator;
    // Constructed with *this: only stores the reference, never dereferences it during
    // GameState's own construction, so binding it before m_factions is populated is safe.
    SecretProjectAvailabilityCalculator m_secretProjectAvailability;
    std::unordered_set<std::string> m_destroyedSecretProjects;
};

} // namespace ac
