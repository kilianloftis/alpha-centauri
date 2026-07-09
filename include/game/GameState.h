#pragma once

#include "game/Faction.h"
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
    GameState();
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
    WorldMap* GetWorldMap();
    const WorldMap* GetWorldMap() const;
    void SetWorldMap(std::unique_ptr<WorldMap> pWorldMap);

    // WorldGlobal lane: every faction's WorldGlobal-scoped active effects, excluding
    // rExclude's own (a faction's own pool already contains its WorldGlobal effects).
    // Turn stages pass this into Faction::ProduceBaseResources / ApplyBaseGrowth so a
    // WorldGlobal effect declared by one faction reaches all of them.
    std::vector<ActiveEffect_t> CollectWorldEffects(const Faction& rExclude) const;

    // Tile effects context (WorldMap + ImprovementRegistry bundled for tile resolution).
    // Must be initialized via InitTileEffects() after SetWorldMap() and registry loading.
    // pUnitComponents sizes the aura scan for unit-projected ThisTile effects; may be null
    // if units never project auras.
    void InitTileEffects(const ImprovementRegistry& rImprovements,
                         const UnitComponentRegistry* pUnitComponents);
    TileEffectsContext& GetTileEffects();
    const TileEffectsContext& GetTileEffects() const;

    UnitOrderExecutor& GetUnitOrderExecutor();

private:
    int m_missionYear;
    // WorldMap and TileEffectsContext are declared before m_factions so they outlive all
    // BaseManagers (which hold TileEffectsContext& references). Members are destroyed in
    // reverse declaration order, so m_factions is destroyed before these two.
    std::unique_ptr<WorldMap> m_worldMap;
    std::unique_ptr<TileEffectsContext> m_pTileEffects;
    std::vector<std::unique_ptr<Faction>> m_factions;
    UnitOrderExecutor m_unitOrderExecutor;
};

} // namespace ac
