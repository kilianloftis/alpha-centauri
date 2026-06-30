#include "game/GameState.h"

#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/Military.h"
#include "game/faction/Diplomacy.h"
#include "game/map/WorldMap.h"

namespace ac
{

GameState::GameState()
    : m_missionYear(2100)
{
}

GameState::~GameState() = default;

int GameState::GetMissionYear() const
{
    return m_missionYear;
}

void GameState::SetMissionYear(int year)
{
    m_missionYear = year;
}

void GameState::IncrementMissionYear()
{
    ++m_missionYear;
}

std::vector<std::unique_ptr<Faction>>& GameState::GetFactions()
{
    return m_factions;
}

const std::vector<std::unique_ptr<Faction>>& GameState::GetFactions() const
{
    return m_factions;
}

void GameState::AddFaction(std::unique_ptr<Faction> pFaction)
{
    m_factions.push_back(std::move(pFaction));
}

int GameState::GetNumFactions() const
{
    return static_cast<int>(m_factions.size());
}

const Faction* GameState::GetPlayerFaction() const
{
    return m_factions.empty() ? nullptr : m_factions[0].get();
}

Faction* GameState::GetPlayerFaction() const
{
    return m_factions.empty() ? nullptr : m_factions[0].get();
}

WorldMap* GameState::GetWorldMap()
{
    return m_worldMap.get();
}

const WorldMap* GameState::GetWorldMap() const
{
    return m_worldMap.get();
}

void GameState::SetWorldMap(std::unique_ptr<WorldMap> pWorldMap)
{
    m_worldMap = std::move(pWorldMap);
}

UnitOrderExecutor& GameState::GetUnitOrderExecutor()
{
    return m_unitOrderExecutor;
}

} // namespace ac
