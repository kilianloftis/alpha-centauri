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
    // 0..100 style rates matching Thinker display. The integer mission/escape rolls use
    // probeStrength / escapeStrength and the Algo/HSA coins, not these percents.
    int successRate = 100;
    int survivalRate = 100;
    int risk = 0;
    int probeStrength = 1;
    int escapeStrength = 1;
    int targetProbeEffect = 0;
    bool bAlgoSave = false;
    bool bHsaCoin = false;
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

// The action's risk, or steal-tech's repeat risk when the base already has stolen research.
// Targeted sabotage at a headquarters, or at Perimeter Defense / Tachyon Field, is at least
// risk 2.
int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId);

// ComputeProbeChances for this probe and target: its effective morale, the target's probe
// defense, the failure scale unless the target BlocksProbeTeams, and the target's success scale.
// Sets bAlgoSave from IgnoresProbeBlock and bHsaCoin from BlocksProbeTeams.
ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk);

// True when the target's Hunter-Seeker would destroy this probe on contact (no Algo).
bool HunterSeekerDestroys(const Unit& rProbe, const ProbeTarget_t& rTarget);

// Floor, exclusive remainder bound (share − floor), and the target treasury for a drain.
struct StealEnergyQuote_t
{
    int floor = 0;
    int remainderBound = 0;
    int treasury = 0;
};

StealEnergyQuote_t QuoteStealEnergy(const BaseManager& rBase, const Faction& rTarget, int morale);

// floor + remainderDraw/2, clamped to the treasury; a result of 1 becomes 0.
int StealEnergyAmount(const StealEnergyQuote_t& rQuote, int remainderDraw);

// Every tech the target has discovered that the actor has not.
std::vector<std::string> StealableTechs(const Faction& rActor, const Faction& rTarget);

// A completed probe mission is attributed; Covert still hides the unit on the map.
bool ProbeDetected(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                   const ProbeTarget_t& rTarget, const ProbeRollResult_t& rRoll);

// False for infiltrate (bit set before the roll) and Total Thought Control.
bool ProbeDeclaresHostileAct(const ProbeActionConfig_t& rAction);

// The action as a hostile act against the target's faction: HostileActAgainst the targeted unit
// or base faction, attributed when ProbeDetected.
HostileAct_t ProbeHostileAct(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                             const ProbeTarget_t& rTarget, const ProbeRollResult_t& rRoll);

bool IsHeadquarters(const BaseManager& rBase);

// Pop removed by a genetic plague at rBase (capped at size − 1).
int GeneticPlagueKilled(const BaseManager& rBase);

// Nearest own base whose tile the probe's domain can occupy. Null when none is legal.
const BaseManager* ProbeReturnBase(const Unit& rProbe);

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
