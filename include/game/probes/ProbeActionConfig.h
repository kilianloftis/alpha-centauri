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

struct ProbeSuccessFormula_t
{
    int moraleDivisor = 2;
    int strengthOffset = 1;
    int defenseClampMin = -2;
    int defenseClampMax = 0;
};

// Paid action tunables. Base cost: (garrison + mindControl / divisor + pop) * ((energy + bias)
// / (dist + distBias)); unit cost: mineralCost * ((energy + bias) / (dist + distBias)).
struct ProbeCostConfig_t
{
    int energyBias = 0;
    int distBias = 0;
    // Base target only, as is garrisonMultiplierBase: the parser requires both there and
    // rejects them on a unit target.
    int mindControlDivisor = 0;
    // The garrison multiplier before each unit's garrison_probe_multiplier (the binary's + 1).
    int garrisonMultiplierBase = 0;
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
    // Incite Drone Riots only: remaining assimilation turns added, and the per-citizen cap.
    int assimilationTurnsAdded = 0;
    double assimilationTurnsCapFactor = 0.0;
    // Sabotage Facility only: the least risk against an HQ or a high_risk_sabotage facility.
    int protectedRisk = 0;
    // Drain Energy only: the floor is share * morale / floorMoraleDivisor, and the remainder
    // draw is divided by remainderDivisor.
    int floorMoraleDivisor = 0;
    int remainderDivisor = 0;
    // Genetic Plague only: a repeat plague divides kills and garrison damage by repeatDivisor;
    // a garrison unit takes hp / garrisonDamageDivisor plus a draw below that.
    int repeatDivisor = 0;
    int garrisonDamageDivisor = 0;
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
    // Baselines injected into every faction's pool (garrison weight, subversion cost scale).
    std::vector<EffectConfig_t> effects;
    std::vector<ProbeActionConfig_t> actions;

    const ProbeActionConfig_t* Find(ProbeActionId_t id) const;
};

} // namespace ac
