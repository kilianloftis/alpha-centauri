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

// World mutations for a successful mission, without rolls, payment, promotion or probe loss.
// False for an action id unknown to rTarget's kind.
bool ApplyProbeActionEffect(Unit& rProbe, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget, GameState& rGameState,
                            const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                            std::mt19937& rRng);

// rAction's on_paid_effects, fired once the cost is paid so a failed attempt still fires them.
void ApplyProbePaidEffects(Unit& rProbe, const ProbeActionConfig_t& rAction,
                           const ProbeTarget_t& rTarget, GameState& rGameState,
                           std::mt19937& rRng);

// Roll mission then escape. RISK 0 missions are always successful (100%).
ProbeRollResult_t RollProbeAction(const ProbeChance_t& rChances, std::mt19937& rRng);

// Intrinsic XP bump after a fully successful probe mission (caps at max).
bool TryPromoteProbeMission(Unit& rProbe, const MoraleCalculator& rMorale);

} // namespace ac
