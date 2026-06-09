#include "game/GameState.h"

#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/base/Economy.h"
#include "game/faction/Military.h"
#include "game/faction/Research.h"
#include "game/faction/Diplomacy.h"

namespace ac
{

GameState::GameState()
    : m_missionYear(2100)
    , m_bShouldExit(false)
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

bool GameState::ShouldExit() const
{
    return m_bShouldExit;
}

void GameState::SetShouldExit(bool bShouldExit)
{
    m_bShouldExit = bShouldExit;
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

} // namespace ac
