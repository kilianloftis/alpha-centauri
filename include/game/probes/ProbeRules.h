#pragma once

#include "game/probes/ProbeActionConfig.h"
#include "game/probes/ProbeTarget.h"

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

// Pure probe mechanics: eligibility, cost quotes, chance/roll math, and effect-stat
// resolves against an already-resolved ProbeTarget_t. Order gates (moves, adjacency),
// payment, mission orchestration, and world mutations belong to ProbeActionExecutor /
// ProbeActionMutations.

// Does the target carry flag, via its own effects, its faction pool, or any of that
// faction's bases? Pool-sourced FactionWide flags (e.g. HSA BlocksProbeTeams) are visible
// via ResolveFlag(Faction), but SE-expanded FactionGlobal flags (e.g.
// ProbeSubversionImmune) only appear on base effect lists, so bases are walked too.
bool TargetHasFlag(const ProbeTarget_t& rTarget, RuleFlagId_t flag);

// Wiki "target base PROBE effect": probe_defense (SE negative Probe rating + Covert Ops,
// etc.) from the target's EffectSourceBase, clamped to [defenseClampMin, defenseClampMax]
// (default [-2, 0]). Positive Probe does not emit probe_defense — it raises own-probe
// morale instead.
int TargetProbeDefenseEffect(const ProbeTarget_t& rTarget,
                             const ProbeSuccessFormula_t& rFormula);

// PureMultiplier resolve of probe_success_scale from the target (1.0 default). HSA halves
// success for enhanced probes.
double ResolveTargetSuccessScale(const ProbeTarget_t& rTarget);

// failureScale / successScale are PureMultiplier resolves (1.0 = unchanged). Thinker:
// failure scale applies only when the target does not BlocksProbeTeams; success scale
// always applies.
ProbeChance_t ComputeProbeChances(int moraleLevel, int risk,
                                  int targetProbeEffect,
                                  const ProbeSuccessFormula_t& rFormula,
                                  double failureScale = 1.0,
                                  double successScale = 1.0);

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

// Probe-mechanics eligibility only: ProbeTeam, required tech, target-kind match, HQ
// constraints, BlocksProbeTeams / ProbeSubversionImmune, and — for paid actions — that a cost
// can actually be quoted, so an action that payment would refuse is never offered. Does not
// check moves or adjacency (those belong to ProbeActionExecutor).
bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                    const ProbeTarget_t& rTarget);

// Energy cost rActor is quoted for a paid action (0 when free). The base formula reads the
// actor's mind-control total. Empty when the formula cannot produce a cost (e.g. zero distance
// to HQ). Affordability / payment live in ProbeActionExecutor.
std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction,
                                        const Faction& rActor, const ProbeTarget_t& rTarget);

} // namespace ac
