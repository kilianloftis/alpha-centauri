#include "game/units/ProbeActionConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace ac
{

namespace
{

ProbeTargetKind_t ParseTarget_(const std::string& rTarget)
{
    if (rTarget == "base")
    {
        return ProbeTargetKind_t::Base;
    }
    if (rTarget == "unit")
    {
        return ProbeTargetKind_t::Unit;
    }
    throw std::runtime_error("probe_actions.json: unknown target '" + rTarget + "'");
}

ProbeCostConfig_t ParseCost_(const json& rCostJson, ProbeTargetKind_t target,
                              const std::string& rActionName)
{
    ProbeCostConfig_t cost;
    cost.energyBias = rCostJson.value("energy_bias", 0);
    cost.distBias = rCostJson.value("dist_bias", 0);
    // Only the base formula reads the divisor, so a unit-target cost would ignore it silently.
    if (target == ProbeTargetKind_t::Unit)
    {
        if (rCostJson.contains("mind_control_divisor"))
        {
            throw std::runtime_error("Probe action '" + rActionName
                                     + "': mind_control_divisor applies only to base targets");
        }
        return cost;
    }
    if (!rCostJson.contains("mind_control_divisor")
        || !rCostJson.at("mind_control_divisor").is_number_integer())
    {
        throw std::runtime_error("Probe action '" + rActionName
                                 + "': cost requires an integer 'mind_control_divisor'");
    }
    cost.mindControlDivisor = rCostJson.at("mind_control_divisor").get<int>();
    if (cost.mindControlDivisor < 1)
    {
        throw std::runtime_error("Probe action '" + rActionName
                                 + "': mind_control_divisor must be >= 1");
    }
    return cost;
}

bool RunsOnSuccessEffects_(ProbeActionId_t id)
{
    switch (id)
    {
        case ProbeActionId_t::Infiltrate:
        case ProbeActionId_t::SabotageRandom:
        case ProbeActionId_t::GeneticPlague:
        case ProbeActionId_t::SubvertUnit:
            return true;
        case ProbeActionId_t::StealTech:
        case ProbeActionId_t::DrainEnergy:
        case ProbeActionId_t::SabotageFacility:
        case ProbeActionId_t::InciteDroneRiots:
        case ProbeActionId_t::Assassinate:
        case ProbeActionId_t::MindControlBase:
        case ProbeActionId_t::TotalThoughtControl:
            return false;
    }
    throw std::logic_error("RunsOnSuccessEffects_: unhandled enumerator");
}

ProbeActionConfig_t ParseAction_(const json& rActionJson)
{
    ProbeActionConfig_t action;
    action.id = ParseProbeActionId(rActionJson.at("id").get<std::string>());
    action.name = rActionJson.value("name", rActionJson.at("id").get<std::string>());
    action.target = ParseTarget_(rActionJson.at("target").get<std::string>());
    action.risk = rActionJson.value("risk", 0);
    if (rActionJson.contains("risk_repeat") && !rActionJson.at("risk_repeat").is_null())
    {
        action.riskRepeat = rActionJson.at("risk_repeat").get<int>();
    }
    action.requiredTech = rActionJson.value("required_tech", "");
    action.bHqOnly = rActionJson.value("hq_only", false);
    action.bNotHq = rActionJson.value("not_hq", false);
    // Only the riot action has a riot duration; accepting the key elsewhere would silently
    // ignore it.
    if (action.id == ProbeActionId_t::InciteDroneRiots)
    {
        if (!rActionJson.contains("riot_turns"))
        {
            throw std::runtime_error("Probe action '" + action.name
                                     + "': missing required field 'riot_turns'");
        }
        action.riotTurns = rActionJson.at("riot_turns").get<int>();
        if (action.riotTurns < 1)
        {
            throw std::runtime_error("Probe action '" + action.name
                                     + "': riot_turns must be >= 1");
        }
    }
    else if (rActionJson.contains("riot_turns"))
    {
        throw std::runtime_error("Probe action '" + action.name
                                 + "': riot_turns applies only to incite_drone_riots");
    }
    if (rActionJson.contains("cost") && !rActionJson.at("cost").is_null())
    {
        action.cost = ParseCost_(rActionJson.at("cost"), action.target, action.name);
    }
    if (rActionJson.contains("effects"))
    {
        nlohmann::json wrapper = nlohmann::json::object();
        wrapper["effects"] = rActionJson.at("effects");
        action.effects = EffectConfigParser::ParseEffects(
            wrapper, EffectSourceKind_t::ProbeAction,
            ProbeActionIdToString(action.id));
    }
    if (rActionJson.contains("on_success_effects") && !RunsOnSuccessEffects_(action.id))
    {
        throw std::runtime_error("Probe action '" + action.name
                                 + "': its handler does not run on_success_effects");
    }
    action.onSuccessEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rActionJson, "on_success_effects", ProbeActionIdToString(action.id));
    if (rActionJson.contains("on_paid_effects") && !action.cost.has_value())
    {
        throw std::runtime_error("Probe action '" + action.name
                                 + "': on_paid_effects requires a cost");
    }
    action.onPaidEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rActionJson, "on_paid_effects", ProbeActionIdToString(action.id));
    return action;
}

} // namespace

const ProbeActionConfig_t* ProbeActionsConfig_t::Find(ProbeActionId_t id) const
{
    for (const ProbeActionConfig_t& rAction : actions)
    {
        if (rAction.id == id)
        {
            return &rAction;
        }
    }
    return nullptr;
}

ProbeActionsConfig_t ProbeActionConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + configPath);
    }

    json root;
    configFile >> root;

    ProbeActionsConfig_t config;
    if (root.contains("success_formula"))
    {
        const json& rFormula = root.at("success_formula");
        config.successFormula.moraleDivisor = rFormula.value("morale_divisor", 2);
        config.successFormula.strengthOffset = rFormula.value("strength_offset", 1);
        config.successFormula.defenseClampMin = rFormula.value("defense_clamp_min", -2);
        config.successFormula.defenseClampMax = rFormula.value("defense_clamp_max", 0);
    }

    if (!root.contains("actions") || !root.at("actions").is_array())
    {
        throw std::runtime_error("probe_actions.json: expected 'actions' array");
    }

    for (const json& rActionJson : root.at("actions"))
    {
        config.actions.push_back(ParseAction_(rActionJson));
    }

    if (config.actions.empty())
    {
        throw std::runtime_error("probe_actions.json: actions must be non-empty");
    }
    return config;
}

} // namespace ac
