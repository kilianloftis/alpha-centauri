#pragma once

#include "game/effects/BonusEffect.h"

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

// Tunables for paid probe actions. Formula is selected by ProbeActionConfig_t::target:
//   Base → (garrison + pop) * (energy + energyBias) / (dist + distBias)
//   Unit → mineralCost * (energy + energyBias) / (dist + distBias)
// Absent cost (nullopt) means the action is free.
struct ProbeCostConfig_t
{
    int energyBias = 0;
    int distBias = 0;
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
    bool bIsAtrocity = false;
    std::optional<ProbeCostConfig_t> cost;
    // Optional Instantaneous effects applied on success (e.g. Infiltration + ActionTarget).
    std::vector<EffectConfig_t> effects;
};

struct ProbeActionsConfig_t
{
    ProbeSuccessFormula_t successFormula;
    std::vector<ProbeActionConfig_t> actions;

    const ProbeActionConfig_t* Find(ProbeActionId_t id) const;
};

} // namespace ac
