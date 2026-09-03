#pragma once

#include "game/TurnStages.h"
#include "game/faction/base/BaseTypes.h"

#include <vector>

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
    // Assimilation, composition, mood forecast, and player notices for one living base.
    void ProcessLivingBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    // Pending riot / golden-age notices for the human player (no-op for AI).
    static void EnqueuePendingMoodNotices_(GameState& rGameState, Faction& rFaction,
                                           BaseManager& rBase);
    // Starve-to-zero bases collected during the living-base pass (cannot raze mid-iteration).
    static void RazeDepopulatedBases_(GameState& rGameState, Faction& rFaction,
                                      const std::vector<BaseId_t>& rDepopulated);
};

} // namespace ac
