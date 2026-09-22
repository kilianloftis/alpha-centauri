#pragma once

#include "game/TurnStages.h"

namespace ac
{

class BaseManager;
class Faction;
class GameState;

class Population : public PerFactionTurnStage
{
public:
    explicit Population(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;

private:
    // Assimilation, composition, and mood forecast for one base. Player mood notices come
    // from PopulationManager::OnWill* (wired at session attach).
    void ProcessBase_(BaseManager& rBase);
};

} // namespace ac
