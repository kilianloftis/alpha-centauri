#pragma once

#include "game/probes/ProbeActionConfig.h"
#include "game/probes/ProbeTarget.h"

#include "game/base/BaseTypes.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Unit;
class BaseManager;

struct ProbeChance_t
{
    // 0..100 style rates matching Thinker display.
    int successRate = 100;
    int survivalRate = 100;
    int risk = 0;
    int probeStrength = 1;
    int targetProbeEffect = 0;
};

struct ProbeRollResult_t
{
    bool missionSucceeded = false;
    bool escaped = false;
    ProbeChance_t chances;
};

// Probe mechanics: eligibility, cost quotes, chances and effect stats for a resolved target.
// Gates, payment and mutations belong to ProbeActionExecutor.

// Whether the target, its faction pool or any of its bases carries the flag; SE-expanded flags
// appear only on base effect lists.
bool TargetHasFlag(const ProbeTarget_t& rTarget, RuleFlagId_t flag);

// Wiki "target base PROBE effect": the target's probe_defense, clamped to the defence range.
// Positive Probe raises own-probe morale instead.
int TargetProbeDefenseEffect(const ProbeTarget_t& rTarget,
                             const ProbeSuccessFormula_t& rFormula);

// PureMultiplier resolve of probe_success_scale from the target (1.0 default). HSA halves
// success for enhanced probes.
double ResolveTargetSuccessScale(const ProbeTarget_t& rTarget);

// PureMultiplier scales. Thinker applies failure scale only without BlocksProbeTeams.
ProbeChance_t ComputeProbeChances(int moraleLevel, int risk,
                                  int targetProbeEffect,
                                  const ProbeSuccessFormula_t& rFormula,
                                  double failureScale,
                                  double successScale);

// The action's risk, or its repeat risk at a base this faction has probed before. Sabotaging a
// chosen facility at a headquarters is at least risk 2.
int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId, bool bRepeatAtBase);

// ComputeProbeChances for this probe and target: its effective morale, the target's probe
// defense, the failure scale unless the target BlocksProbeTeams, and the target's success scale.
ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk);

// A drain takes the base's share of the target treasury, at least a morale-tenth of it, plus
// half of what is left of the share, never more than the treasury holds.
int StealEnergyAmount(const BaseManager& rBase, const Faction& rTarget, int morale);

// Techs the actor could research now that the target already knows.
std::vector<std::string> StealableTechs(const Faction& rActor, const Faction& rTarget);

// Whether the target faction detects the probe and so learns who sent a covert one.
// TODO: the detection roll ("getting away cleanly") is not implemented; nothing is detected.
bool ProbeDetected(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                   const ProbeTarget_t& rTarget, const ProbeRollResult_t& rRoll);

// The action as a hostile act against the target's faction: HostileActAgainst the targeted unit
// or base faction, and attributed as well when ProbeDetected.
HostileAct_t ProbeHostileAct(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                             const ProbeTarget_t& rTarget, const ProbeRollResult_t& rRoll);

bool IsHeadquarters(const BaseManager& rBase);

// Probe eligibility, including that a paid action's cost can be quoted. Moves and adjacency
// are ProbeActionExecutor's.
bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                    const ProbeTarget_t& rTarget);

// SMAC popups when the current player is the probe owner or the target owner.
bool ProbeNoticeReachesPlayer(FactionId_t actorId, FactionId_t targetId);

// The energy cost quoted to rActor, 0 when free; empty when the formula has no answer.
std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction,
                                        const Faction& rActor, const ProbeTarget_t& rTarget);

} // namespace ac
