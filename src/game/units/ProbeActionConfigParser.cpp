#include "game/units/ProbeActionConfigParser.h"

#include "game/effects/EffectConfigParser.h"

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

ProbeCostConfig_t ParseCost_(const json& rCostJson)
{
    ProbeCostConfig_t cost;
    cost.energyBias = rCostJson.value("energy_bias", 0);
    cost.distBias = rCostJson.value("dist_bias", 0);
    return cost;
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
    action.bIsAtrocity = rActionJson.value("atrocity", false);
    if (rActionJson.contains("cost") && !rActionJson.at("cost").is_null())
    {
        action.cost = ParseCost_(rActionJson.at("cost"));
    }
    if (rActionJson.contains("effects"))
    {
        nlohmann::json wrapper = nlohmann::json::object();
        wrapper["effects"] = rActionJson.at("effects");
        action.effects = EffectConfigParser::ParseEffects(
            wrapper, EffectSourceKind_t::ProbeAction,
            ProbeActionIdToString(action.id));
    }
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
