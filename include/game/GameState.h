#pragma once

#include "game/Faction.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitOrderExecutor.h"
#include <memory>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TileEffectsContext;

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
    std::vector<std::unique_ptr<Faction>>& GetFactions();
    const std::vector<std::unique_ptr<Faction>>& GetFactions() const;
    void AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;
    const Faction* GetPlayerFaction() const;
    Faction* GetPlayerFaction();

    // World map
    WorldMap* GetWorldMap();
    const WorldMap* GetWorldMap() const;
    void SetWorldMap(std::unique_ptr<WorldMap> pWorldMap);

    // Tile effects context (WorldMap + ImprovementRegistry bundled for tile resolution).
    // Must be initialized via InitTileEffects() after SetWorldMap() and registry loading.
    void InitTileEffects(const ImprovementRegistry& rImprovements);
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
