#pragma once

#include "game/Faction.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitOrderExecutor.h"
#include "lib/DerefView.h"
#include <memory>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TileEffectsContext;
class UnitComponentRegistry;

class GameState
{
public:
    // pUnitComponents sizes the aura scan for unit-projected ThisTile effects; may be null
    // if units never project auras. Throws if pWorldMap is null.
    GameState(std::unique_ptr<WorldMap> pWorldMap,
              const ImprovementRegistry& rImprovements,
              const UnitComponentRegistry* pUnitComponents);
    ~GameState();

    // Mission year
    int GetMissionYear() const;
    void SetMissionYear(int year);
    void IncrementMissionYear();

    // Factions
    Faction& AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;
    // Iterate factions by reference without exposing the owning unique_ptrs.
    auto Factions() { return DerefView(m_factions); }
    auto Factions() const { return DerefView(m_factions); }
    const Faction* GetPlayerFaction() const;
    Faction* GetPlayerFaction();

    // World map
    WorldMap& GetWorldMap();
    const WorldMap& GetWorldMap() const;

    // WorldGlobal lane: every faction's WorldGlobal-scoped active effects, excluding
    // rExclude's own (a faction's own pool already contains its WorldGlobal effects).
    // Turn stages pass this into Faction::ProduceBaseResources / ApplyBaseGrowth so a
    // WorldGlobal effect declared by one faction reaches all of them.
    std::vector<ActiveEffect_t> CollectWorldEffects(const Faction& rExclude) const;

    // Tile effects context (WorldMap + ImprovementRegistry bundled for tile resolution).
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    UnitOrderExecutor& GetUnitOrderExecutor();

    // Scans all bases of all factions to check whether a secret project has already been
    // built. Owned here (rather than GameDataContext) because it queries this live,
    // mutable faction data — an "immutable definition data" object referencing it would be
    // constructible only after GameState exists, and would dangle if GameState were ever
    // rebuilt (new game / load game).
    const SecretProjectAvailabilityCalculator& GetSecretProjectAvailability() const;

private:
    int m_missionYear;
    // WorldMap and TileEffectsContext are declared before m_factions so they outlive all
    // BaseManagers (which hold TileEffectsContext& references). Members are destroyed in
    // reverse declaration order, so m_factions is destroyed before these two.
    std::unique_ptr<WorldMap> m_worldMap;
    std::unique_ptr<TileEffectsContext> m_pTileEffects;
    std::vector<std::unique_ptr<Faction>> m_factions;
    UnitOrderExecutor m_unitOrderExecutor;
    // Constructed with *this: only stores the reference, never dereferences it during
    // GameState's own construction, so binding it before m_factions is populated is safe.
    SecretProjectAvailabilityCalculator m_secretProjectAvailability;
};

} // namespace ac
