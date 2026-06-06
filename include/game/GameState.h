#pragma once

#include "game/Faction.h"
#include "lib/Signal.h"
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

    // Exit flag
    bool ShouldExit() const;
    void SetShouldExit(bool bShouldExit);

    // Factions
    std::vector<std::unique_ptr<Faction>>& GetFactions();
    const std::vector<std::unique_ptr<Faction>>& GetFactions() const;
    void AddFaction(std::unique_ptr<Faction> pFaction);
    int GetNumFactions() const;

    // Signals
    Signal<int> on_turn_started;

private:
    int m_missionYear;
    bool m_bShouldExit;
    std::vector<std::unique_ptr<Faction>> m_factions;
};

} // namespace ac
