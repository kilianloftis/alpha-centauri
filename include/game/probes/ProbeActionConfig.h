#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

enum class ProbeTargetKind_t
{
    Base,
    Unit,
};

// Fixed C++ probe handlers. JSON action "id" strings parse to these.
enum class ProbeActionId_t
{
    Infiltrate,
    StealTech,
    DrainEnergy,
    SabotageRandom,
    SabotageFacility,
    InciteDroneRiots,
    Assassinate,
    MindControlBase,
    TotalThoughtControl,
    GeneticPlague,
    SubvertUnit,
};

inline const char* ProbeActionIdToString(ProbeActionId_t id)
{
    switch (id)
    {
        case ProbeActionId_t::Infiltrate:          return "infiltrate";
        case ProbeActionId_t::StealTech:           return "steal_tech";
        case ProbeActionId_t::DrainEnergy:         return "drain_energy";
        case ProbeActionId_t::SabotageRandom:      return "sabotage_random";
        case ProbeActionId_t::SabotageFacility:    return "sabotage_facility";
        case ProbeActionId_t::InciteDroneRiots:    return "incite_drone_riots";
        case ProbeActionId_t::Assassinate:         return "assassinate";
        case ProbeActionId_t::MindControlBase:     return "mind_control_base";
        case ProbeActionId_t::TotalThoughtControl: return "total_thought_control";
        case ProbeActionId_t::GeneticPlague:       return "genetic_plague";
        case ProbeActionId_t::SubvertUnit:         return "subvert_unit";
    }
    throw std::logic_error("ProbeActionIdToString: unhandled enumerator");
}

inline ProbeActionId_t ParseProbeActionId(const std::string& rId)
{
    if (rId == "infiltrate")            return ProbeActionId_t::Infiltrate;
    if (rId == "steal_tech")            return ProbeActionId_t::StealTech;
    if (rId == "drain_energy")          return ProbeActionId_t::DrainEnergy;
    if (rId == "sabotage_random")       return ProbeActionId_t::SabotageRandom;
    if (rId == "sabotage_facility")     return ProbeActionId_t::SabotageFacility;
    if (rId == "incite_drone_riots")    return ProbeActionId_t::InciteDroneRiots;
    if (rId == "assassinate")           return ProbeActionId_t::Assassinate;
    if (rId == "mind_control_base")     return ProbeActionId_t::MindControlBase;
    if (rId == "total_thought_control") return ProbeActionId_t::TotalThoughtControl;
    if (rId == "genetic_plague")        return ProbeActionId_t::GeneticPlague;
    if (rId == "subvert_unit")          return ProbeActionId_t::SubvertUnit;
    throw std::runtime_error("probe_actions.json: unknown action id '" + rId + "'");
}

// The odds' formulas, Lua expressions ProbeCalculator evaluates: the target's probe defense from
// its probe_defense stat, and the mission and escape draw strengths from the probe's morale and
// that defense. The shown rates are the draws' probabilities, so they need no formulas.
struct ProbeSuccessFormula_t
{
    std::string defense;
    std::string missionStrength;
    std::string escapeStrength;
};

// A paid action's cost before the effect multipliers: a Lua expression ProbeCalculator
// evaluates. A base target's reads MindControlCostInputs_t's variables, a unit target's
// SubvertCostInputs_t's.
struct ProbeCostConfig_t
{
    std::string formula;
};

// Drain Energy's formulas, Lua expressions ProbeCalculator evaluates: the target base's share of
// the treasury, the floor a probe of this morale takes of it, and the amount for a remainder draw.
struct DrainFormulas_t
{
    std::string share;
    std::string floor;
    std::string amount;
};

// Genetic Plague's formulas: pop killed, and each garrison unit's damage and its draw bound.
struct PlagueFormulas_t
{
    std::string kills;
    std::string garrisonDamageBound;
    std::string garrisonDamage;
};

struct ProbeActionConfig_t
{
    ProbeActionId_t id = ProbeActionId_t::Infiltrate;
    std::string name;
    ProbeTargetKind_t target = ProbeTargetKind_t::Base;
    int risk = 0;
    // When set, subsequent attempts at the same base use this RISK (steal tech, etc.).
    std::optional<int> riskRepeat;
    std::string requiredTech;
    bool bHqOnly = false;
    bool bNotHq = false;
    // The victim may keep the current status. False: the agreement always breaks (mind-control
    // city's act_of_aggression).
    bool bMayKeepAgreement = false;
    // If the agreement breaks: ApplyHostileAct (Vendetta, agreement-break blemish, Pact
    // obligations). Otherwise a Pact steps down to Treaty, and Truce/Treaty become Vendetta with
    // no blemish and no obligations. SMAC action_id >= 6.
    bool bHostileActOnBreak = false;
    // A human actor against an AI may implicate another faction (terranx-map.md Frame).
    bool bMayFrame = false;
    // Incite Drone Riots only: remaining assimilation turns added, and the per-citizen cap.
    int assimilationTurnsAdded = 0;
    double assimilationTurnsCapFactor = 0.0;
    // Sabotage Facility only: the least risk against an HQ or a high_risk_sabotage facility.
    int protectedRisk = 0;
    // Drain Energy only.
    std::optional<DrainFormulas_t> drain;
    // Genetic Plague only.
    std::optional<PlagueFormulas_t> plague;
    std::optional<ProbeCostConfig_t> cost;
    // Continuous effects the action declares. Probe actions have no pool origin, so this is
    // effectively unused today; the mission's real work is in onSuccessEffects.
    std::vector<EffectConfig_t> effects;
    // One-shot effects on success; only actions whose handlers run them accept the list.
    std::vector<TriggeredEffectConfig_t> onSuccessEffects;
    // One-shot effects applied once the cost is paid, before the success roll, so a failed
    // attempt still fires them (mind control's RecordMindControl). Paid actions only.
    std::vector<TriggeredEffectConfig_t> onPaidEffects;
};

struct ProbeActionsConfig_t
{
    ProbeSuccessFormula_t successFormula;
    // Added to the mission risk when the probe implicates another faction.
    int frameRisk = 0;
    // Baselines injected into every faction's pool (garrison weight, subversion cost scale).
    std::vector<EffectConfig_t> effects;
    std::vector<ProbeActionConfig_t> actions;

    const ProbeActionConfig_t* Find(ProbeActionId_t id) const;
};

} // namespace ac
