#pragma once

#include "game/Faction.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitOrderExecutor.h"
#include <memory>
#include <vector>

namespace ac
{

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
    Faction* GetPlayerFaction() const;

    // World map
    WorldMap* GetWorldMap();
    const WorldMap* GetWorldMap() const;
    void SetWorldMap(std::unique_ptr<WorldMap> pWorldMap);

    UnitOrderExecutor& GetUnitOrderExecutor();

private:
    int m_missionYear;
    std::vector<std::unique_ptr<Faction>> m_factions;
    std::unique_ptr<WorldMap> m_worldMap;
    UnitOrderExecutor m_unitOrderExecutor;
};

} // namespace ac
