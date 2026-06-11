#pragma once

#include "game/Faction.h"
#include "game/map/WorldMap.h"
#include <vector>
#include <memory>

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

    // World map
    WorldMap* GetWorldMap();
    const WorldMap* GetWorldMap() const;
    void SetWorldMap(std::unique_ptr<WorldMap> pWorldMap);

private:
    int m_missionYear;
    std::vector<std::unique_ptr<Faction>> m_factions;
    std::unique_ptr<WorldMap> m_worldMap;
};

} // namespace ac
