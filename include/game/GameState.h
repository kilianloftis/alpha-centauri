#pragma once

#include "game/Faction.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/FirstContactResolver.h"
#include "game/map/WorldMap.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Pathfinder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/ProbeActionExecutor.h"
#include "lib/DerefView.h"
#include "lib/IdAllocator.h"
#include "lib/Signal.h"
#include <memory>
#include <random>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TileEffectsContext;
class UnitComponentRegistry;
class EventBus;
class GameSettings;

class GameState
{
public:
    // pUnitComponents sizes the aura scan for unit-projected ThisTile effects; may be null
    // if units never project auras. Throws if pWorldMap is null.
    // rSettings is a non-owning reference to Engine-owned player preferences (not save state).
    // rMorale is the GameDataContext-owned calculator (XP ranks / combat % / promotion),
    // borrowed here for the combat and probe paths. It must outlive this GameState.
    GameState(std::unique_ptr<WorldMap> pWorldMap,
              const ImprovementRegistry& rImprovements,
              const UnitComponentRegistry* pUnitComponents,
              GameSettings& rSettings,
              const MoraleCalculator& rMorale);
    ~GameState();

    const MoraleCalculator& GetMoraleCalculator() const;

    // Mission year
    int GetMissionYear() const;
    void SetMissionYear(int year);
    void IncrementMissionYear();

    GameSettings& GetSettings();
    const GameSettings& GetSettings() const;

    EventBus& GetEventBus();
    const EventBus& GetEventBus() const;

    // Factions
    Faction& AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;
    // Iterate factions by reference without exposing the owning unique_ptrs.
    auto Factions() { return DerefView(m_factions); }
    auto Factions() const { return DerefView(m_factions); }
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
    BaseManager* FindBaseAt(int tileX, int tileY);
    const BaseManager* FindBaseAt(int tileX, int tileY) const;

    // WorldGlobal lane: every faction's WorldGlobal-scoped active effects, excluding
    // rExclude's own (a faction's own pool already contains its WorldGlobal effects).
    // Turn stages pass this into Faction::ProduceBaseResources / ApplyBaseGrowth so a
    // WorldGlobal effect declared by one faction reaches all of them.
    std::vector<ActiveEffect_t> CollectWorldEffects(const Faction& rExclude) const;

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

    // Recompute world territory from every faction's bases. Also wired as each faction's
    // OnBaseListChanged handler so founding a base keeps ownership current.
    void RebuildTerritory();

    // Scans all bases of all factions to check whether a secret project has already been
    // built. Owned here (rather than GameDataContext) because it queries this live,
    // mutable faction data — an "immutable definition data" object referencing it would be
    // constructible only after GameState exists, and would dangle if GameState were ever
    // rebuilt (new game / load game).
    const SecretProjectAvailabilityCalculator& GetSecretProjectAvailability() const;

private:
    void OnVisibilitySettingsChanged_();

    int m_missionYear;
    GameSettings& m_rSettings;
    const MoraleCalculator& m_rMorale;
    Signal<>::ScopedConnection m_visibilitySettingsChanged;
    std::unique_ptr<EventBus> m_pEventBus;
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
    IdAllocator m_factionIdAllocator;
    IdAllocator m_baseIdAllocator;
    IdAllocator m_unitIdAllocator;
    // Constructed with *this: only stores the reference, never dereferences it during
    // GameState's own construction, so binding it before m_factions is populated is safe.
    SecretProjectAvailabilityCalculator m_secretProjectAvailability;
};

} // namespace ac
