#pragma once

#include "game/units/ProbeActionConfig.h"
#include "game/units/ProbeActionResult.h"
#include "game/units/ProbeTarget.h"

#include <random>

namespace ac
{

class Unit;
class GameState;
struct GameDataContext;

// Apply the world mutations for a successful probe mission (tech theft, mind control,
// sabotage, etc.). Does not roll success/escape, pay energy, promote, or destroy the probe.
// Returns false when the action id is unknown for rTarget's kind.
// rRng is used for random selection among eligible stealable techs / facilities.
bool ApplyProbeActionEffect(Unit& rProbe, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget, GameState& rGameState,
                            const GameDataContext& rDataContext,
                            const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                            std::mt19937& rRng);

} // namespace ac
