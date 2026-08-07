#pragma once

#include "game/units/ProbeActionConfig.h"
#include "game/units/ProbeTarget.h"

#include "game/effects/EffectEnums.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>

namespace ac
{

class Unit;
class BaseManager;
class WorldMap;
class MoraleCalculator;

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
// ProbeActionEffects.

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

// Roll mission then escape. RISK 0 missions are always successful (100%).
ProbeRollResult_t RollProbeAction(const ProbeChance_t& rChances, std::mt19937& rRng);

bool IsHeadquarters(const BaseManager& rBase);

// Probe-mechanics eligibility only: ProbeTeam, required tech, target-kind match, HQ
// constraints, BlocksProbeTeams / ProbeSubversionImmune, and — for paid actions — that a cost
// can actually be quoted, so an action that payment would refuse is never offered. Does not
// check moves or adjacency (those belong to ProbeActionExecutor). rMap is needed for the
// distance-to-HQ term in the quote.
bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                    const ProbeTarget_t& rTarget, const WorldMap& rMap);

// Energy cost for paid actions (0 when free). Empty when the formula cannot produce a
// cost (e.g. zero distance to HQ). Affordability / payment live in ProbeActionExecutor.
std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction,
                                        const ProbeTarget_t& rTarget,
                                        const WorldMap& rMap);

// Intrinsic XP bump after a fully successful probe mission (caps at max).
bool TryPromoteProbeMission(Unit& rProbe, const MoraleCalculator& rMorale);

} // namespace ac
