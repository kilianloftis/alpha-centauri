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
    // probeStrength / escapeStrength and the save and bust chances, not these percents.
    int successRate = 100;
    int survivalRate = 100;
    int risk = 0;
    int probeStrength = 1;
    int escapeStrength = 1;
    // Percent chance a failed mission or escape draw is saved.
    int failureSaveChance = 0;
    // Percent chance a successful mission or escape draw is busted anyway.
    int successBustChance = 0;
};

struct ProbeRollResult_t
{
    bool missionSucceeded = false;
    bool escaped = false;
    ProbeChance_t chances;
};

// Probe mechanics: eligibility, cost quotes, chances and effect stats for a resolved target.
// Gates, payment and mutations belong to ProbeActionExecutor.

// Wiki "target base PROBE effect": success_formula's defense over the target's probe_defense.
// Positive Probe raises own-probe morale instead.
int TargetProbeDefenseEffect(const ProbeTarget_t& rTarget,
                             const ProbeSuccessFormula_t& rFormula);

// PureMultiplier resolve of probe_success_scale from the target (1.0 default). It applies
// whatever the hunter/intrusion contest decides.
double ResolveTargetSuccessScale(const ProbeTarget_t& rTarget);

// The shown rates are the draws' own odds (MissionDrawSucceeds, EscapeDrawSucceeds), then the
// PureMultiplier scales; ProbeChancesFor decides which apply. Negative risk shows 100%.
ProbeChance_t ComputeProbeChances(int probeStrength, int escapeStrength, int risk,
                                  double failureScale, double successScale);

// The action's risk, or steal-tech's repeat risk when the base already has stolen research.
// Targeted sabotage at a headquarters, or at a high_risk_sabotage facility, is at least the
// action's protected_risk. Implicating framedId adds frame_risk.
int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId, std::optional<FactionId_t> framedId);

// ComputeProbeChances for this probe and target: success_formula's strengths from its effective
// morale and the target's defense, the target's success scale, and the target's bust chance. The probe's failure scale and save
// chance apply unless the target's probe_hunter neutralises it: hunter > 0 and hunter >= the
// probe's probe_intrusion.
ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk);

// True when the target's probe_hunter exceeds the probe's probe_intrusion: the probe dies on
// contact, before any menu, cost or roll.
bool ProbeContactDestroys(const Unit& rProbe, const ProbeTarget_t& rTarget);

// The source id of the largest probe_hunter contribution on the target's effect source. Only
// meaningful once ProbeContactDestroys holds; throws when nothing contributes.
std::string ProbeHunterSource(const ProbeTarget_t& rTarget);

// Floor, exclusive remainder bound (share − floor), and the target treasury for a drain.
struct StealEnergyQuote_t
{
    int floor = 0;
    int remainderBound = 0;
    int treasury = 0;
};

// The share and floor come from the action's share_formula and floor_formula.
StealEnergyQuote_t QuoteStealEnergy(const ProbeActionConfig_t& rAction, const BaseManager& rBase,
                                    const Faction& rTarget, int morale);

// The action's amount_formula for this draw, clamped to the treasury; a result of 1 becomes 0.
int StealEnergyAmount(const ProbeActionConfig_t& rAction, const StealEnergyQuote_t& rQuote,
                      int remainderDraw);

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

// Standing write when the victim does not keep: the agreement breaks, or there was none
// (terranx-map.md "Agreement aftermath").
enum class BreakOutcome_t
{
    // ApplyHostileAct: no agreement to break, or the action's hostile_act_on_break.
    HostileAct,
    // The Pact falls to Treaty with no blemish.
    PactEnded,
    // ApplyProbeVendetta: Vendetta with no blemish and no obligations.
    Vendetta,
};

struct ProbeAftermath_t
{
    // False when the break is forced: no agreement, or the action's may_keep_agreement is false.
    bool bMayKeepAgreement = false;
    // Keeping adds a draw in [0, keepFrictionDraw) to the victim's friction toward the actor.
    int keepFrictionDraw = 0;
    BreakOutcome_t breakOutcome = BreakOutcome_t::HostileAct;
};

// Decided before any Vendetta, since keeping an agreement needs the agreement in place. Only a
// pair holding Truce, Treaty or Pact over an attributed act has an agreement to keep.
ProbeAftermath_t ResolveProbeAftermath(const HostileAct_t& rAct, const ProbeActionConfig_t& rAction);

// Whether rActor may implicate another faction in rAction against rTarget: the action's
// may_frame, a player actor and an AI target, both with diplomacy.
bool ProbeMayFrame(const Faction& rActor, const Faction& rTarget,
                   const ProbeActionConfig_t& rAction);

// The factions rActor may implicate against rTarget: every other living faction with diplomacy
// that rActor knows. No Vendetta filter.
std::vector<FactionId_t> FrameCandidates(const Faction& rActor, const Faction& rTarget);

enum class ProbeFrameOutcome_t
{
    // The victim blames the framed faction, and its keep-or-break toward the actor is skipped.
    Framed,
    // The frame failed: the framed faction turns on the actor.
    Flopped,
};

struct ProbeFrame_t
{
    ProbeFrameOutcome_t outcome = ProbeFrameOutcome_t::Framed;
    FactionId_t framedId = 0;
    // ApplyProbeVendetta's pair: the victim against the framed faction, or on a flop the framed
    // faction against the actor.
    FactionId_t avenger = 0;
    FactionId_t avengedOn = 0;
};

// A successful mission and escape frames framedId; anything else flops. Empty without a frame.
// Unlike the binary, the actor and victim need not hold an agreement.
std::optional<ProbeFrame_t> ResolveProbeFrame(const HostileAct_t& rAct,
                                              std::optional<FactionId_t> framedId,
                                              const ProbeRollResult_t& rRoll);

// Every standing write a hostile probe makes, in order: the frame's Vendetta, then the victim's
// keep-or-break toward the actor.
struct ProbeStanding_t
{
    std::optional<ProbeFrame_t> frame;
    std::optional<ProbeAftermath_t> aftermath;
};

// A frame that held spares the actor the aftermath; a flop or no frame does not.
ProbeStanding_t ResolveProbeStanding(const HostileAct_t& rAct, const ProbeActionConfig_t& rAction,
                                     std::optional<FactionId_t> framedId,
                                     const ProbeRollResult_t& rRoll);

// Pop removed by a genetic plague at rBase: the action's kill_formula times the base's
// plague_damage_scale, capped at size − 1.
int GeneticPlagueKilled(const ProbeActionConfig_t& rAction, const BaseManager& rBase);

// The exclusive bound of the draw a plague adds to a garrison unit's damage (the action's
// garrison_damage_bound_formula).
int PlagueGarrisonDrawBound(const ProbeActionConfig_t& rAction, int hp);

// The action's garrison_damage_formula for this draw, times the base's plague_damage_scale.
int PlagueGarrisonDamage(const ProbeActionConfig_t& rAction, const BaseManager& rBase, int hp,
                         int draw);

// The binary's mission check: a draw in [0, probeStrength) at or above the risk succeeds.
bool MissionDrawSucceeds(const ProbeChance_t& rChances, int draw);

// The escape check is strict: a draw in [0, escapeStrength) must exceed the risk.
bool EscapeDrawSucceeds(const ProbeChance_t& rChances, int draw);

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
