#pragma once

#include "game/probes/ProbeActionConfig.h"
#include "game/probes/ProbeActionResult.h"
#include "game/probes/ProbeRules.h"
#include "game/probes/ProbeTarget.h"

#include <random>

namespace ac
{

class GameState;
class MoraleCalculator;
class Unit;

// Apply the world mutations for a successful probe mission (tech theft, mind control,
// sabotage, etc.). Does not roll success/escape, pay energy, promote, or destroy the probe.
// Returns false when the action id is unknown for rTarget's kind.
// rRng is used for random selection among eligible stealable techs / facilities.
bool ApplyProbeActionEffect(Unit& rProbe, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget, GameState& rGameState,
                            const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                            std::mt19937& rRng);

// Apply rAction's on_paid_effects once its cost is paid, before the success roll, so a failed
// attempt still fires them. The actor is the subject faction and rTarget's owner the action
// target.
void ApplyProbePaidEffects(Unit& rProbe, const ProbeActionConfig_t& rAction,
                           const ProbeTarget_t& rTarget, GameState& rGameState,
                           std::mt19937& rRng);

// Roll mission then escape. RISK 0 missions are always successful (100%).
ProbeRollResult_t RollProbeAction(const ProbeChance_t& rChances, std::mt19937& rRng);

// Intrinsic XP bump after a fully successful probe mission (caps at max).
bool TryPromoteProbeMission(Unit& rProbe, const MoraleCalculator& rMorale);

} // namespace ac
