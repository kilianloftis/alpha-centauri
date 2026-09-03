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
    // Assimilation, composition, mood forecast, and player notices for one base.
    void ProcessBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    // Pending riot / golden-age notices for the human player (no-op for AI).
    static void EnqueuePendingMoodNotices_(GameState& rGameState, Faction& rFaction,
                                           BaseManager& rBase);
};

} // namespace ac
