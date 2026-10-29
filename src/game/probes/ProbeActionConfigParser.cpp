#include "game/probes/ProbeActionConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

// A required Lua expression that ProbeCalculator evaluates; rContext names its owner in errors.
std::string RequireFormula_(const json& rJson, const std::string& rKey,
                            const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_string()
        || rJson.at(rKey).get<std::string>().empty())
    {
        throw std::runtime_error(rContext + ": requires a formula string '" + rKey + "'");
    }
    return rJson.at(rKey).get<std::string>();
}

std::string ActionContext_(const std::string& rActionName)
{
    return "Probe action '" + rActionName + "'";
}

bool RequireBool_(const json& rJson, const std::string& rKey, const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_boolean())
    {
        throw std::runtime_error(rContext + ": requires a boolean '" + rKey + "'");
    }
    return rJson.at(rKey).get<bool>();
}

ProbeCostConfig_t ParseCost_(const json& rCostJson, const std::string& rActionName)
{
    ProbeCostConfig_t cost;
    cost.formula = RequireFormula_(rCostJson, "formula", ActionContext_(rActionName));
    return cost;
}

// An integer one action's rule reads: required (at least `minimum`) on that action, rejected on
// any other, which would ignore it.
int ParseActionInt_(const json& rActionJson, const ProbeActionConfig_t& rAction,
                    ProbeActionId_t owner, const std::string& rKey, int minimum)
{
    if (rAction.id != owner)
    {
        if (rActionJson.contains(rKey))
        {
            throw std::runtime_error("Probe action '" + rAction.name + "': " + rKey
                                     + " applies only to " + ProbeActionIdToString(owner));
        }
        return 0;
    }
    if (!rActionJson.contains(rKey) || !rActionJson.at(rKey).is_number_integer())
    {
        throw std::runtime_error("Probe action '" + rAction.name
                                 + "': missing required integer '" + rKey + "'");
    }
    const int value = rActionJson.at(rKey).get<int>();
    if (value < minimum)
    {
        throw std::runtime_error("Probe action '" + rAction.name + "': " + rKey + " must be >= "
                                 + std::to_string(minimum));
    }
    return value;
}

// Formulas one action's rules read: rejected on any other, which would ignore them.
bool OwnsKeys_(const json& rActionJson, const ProbeActionConfig_t& rAction, ProbeActionId_t owner,
               const std::vector<std::string>& rKeys)
{
    if (rAction.id == owner)
    {
        return true;
    }
    for (const std::string& rKey : rKeys)
    {
        if (rActionJson.contains(rKey))
        {
            throw std::runtime_error("Probe action '" + rAction.name + "': " + rKey
                                     + " applies only to " + ProbeActionIdToString(owner));
        }
    }
    return false;
}

std::optional<DrainFormulas_t> ParseDrainFormulas_(const json& rActionJson,
                                                   const ProbeActionConfig_t& rAction)
{
    if (!OwnsKeys_(rActionJson, rAction, ProbeActionId_t::DrainEnergy,
                   {"share_formula", "floor_formula", "amount_formula"}))
    {
        return std::nullopt;
    }
    const std::string context = ActionContext_(rAction.name);
    return DrainFormulas_t{RequireFormula_(rActionJson, "share_formula", context),
                           RequireFormula_(rActionJson, "floor_formula", context),
                           RequireFormula_(rActionJson, "amount_formula", context)};
}

std::optional<PlagueFormulas_t> ParsePlagueFormulas_(const json& rActionJson,
                                                     const ProbeActionConfig_t& rAction)
{
    if (!OwnsKeys_(rActionJson, rAction, ProbeActionId_t::GeneticPlague,
                   {"kill_formula", "garrison_damage_bound_formula", "garrison_damage_formula"}))
    {
        return std::nullopt;
    }
    const std::string context = ActionContext_(rAction.name);
    return PlagueFormulas_t{RequireFormula_(rActionJson, "kill_formula", context),
                            RequireFormula_(rActionJson, "garrison_damage_bound_formula", context),
                            RequireFormula_(rActionJson, "garrison_damage_formula", context)};
}

void ParseActionNumbers_(const json& rActionJson, ProbeActionConfig_t& rAction)
{
    rAction.protectedRisk = ParseActionInt_(rActionJson, rAction,
                                            ProbeActionId_t::SabotageFacility, "protected_risk", 0);
    rAction.drain = ParseDrainFormulas_(rActionJson, rAction);
    rAction.plague = ParsePlagueFormulas_(rActionJson, rAction);
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

void ParseInciteAssimilation_(const json& rActionJson, ProbeActionConfig_t& rAction)
{
    if (!rActionJson.contains("assimilation_turns_added")
        || !rActionJson.at("assimilation_turns_added").is_number_integer())
    {
        throw std::runtime_error("Probe action '" + rAction.name
                                 + "': missing required integer 'assimilation_turns_added'");
    }
    rAction.assimilationTurnsAdded = rActionJson.at("assimilation_turns_added").get<int>();
    if (rAction.assimilationTurnsAdded < 1)
    {
        throw std::runtime_error("Probe action '" + rAction.name
                                 + "': assimilation_turns_added must be >= 1");
    }
    if (!rActionJson.contains("assimilation_turns_cap_factor")
        || !rActionJson.at("assimilation_turns_cap_factor").is_number())
    {
        throw std::runtime_error("Probe action '" + rAction.name
                                 + "': missing required number 'assimilation_turns_cap_factor'");
    }
    rAction.assimilationTurnsCapFactor =
        rActionJson.at("assimilation_turns_cap_factor").get<double>();
    if (rAction.assimilationTurnsCapFactor <= 0.0)
    {
        throw std::runtime_error("Probe action '" + rAction.name
                                 + "': assimilation_turns_cap_factor must be > 0");
    }
}

void RejectInciteKeys_(const json& rActionJson, const std::string& rName)
{
    if (rActionJson.contains("assimilation_turns_added")
        || rActionJson.contains("assimilation_turns_cap_factor"))
    {
        throw std::runtime_error("Probe action '" + rName
                                 + "': assimilation_turns_* applies only to incite_drone_riots");
    }
}

std::vector<EffectConfig_t> ParseActionEffects_(const json& rActionJson, ProbeActionId_t id)
{
    if (!rActionJson.contains("effects"))
    {
        return {};
    }
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rActionJson.at("effects");
    return EffectConfigParser::ParseEffects(wrapper, EffectSourceKind_t::ProbeAction,
                                            ProbeActionIdToString(id));
}

ProbeActionConfig_t ParseAction_(const json& rActionJson)
{
    ProbeActionConfig_t action;
    action.id = ParseProbeActionId(rActionJson.at("id").get<std::string>());
    action.name = rActionJson.value("name", rActionJson.at("id").get<std::string>());
    action.target = ParseTarget_(rActionJson.at("target").get<std::string>());
    const std::string context = ActionContext_(action.name);
    action.bMayKeepAgreement = RequireBool_(rActionJson, "may_keep_agreement", context);
    action.bHostileActOnBreak = RequireBool_(rActionJson, "hostile_act_on_break", context);
    action.bMayFrame = RequireBool_(rActionJson, "may_frame", context);
    action.risk = rActionJson.value("risk", 0);
    if (rActionJson.contains("risk_repeat") && !rActionJson.at("risk_repeat").is_null())
    {
        action.riskRepeat = rActionJson.at("risk_repeat").get<int>();
    }
    action.requiredTech = rActionJson.value("required_tech", "");
    action.bHqOnly = rActionJson.value("hq_only", false);
    action.bNotHq = rActionJson.value("not_hq", false);
    if (action.id == ProbeActionId_t::InciteDroneRiots)
    {
        ParseInciteAssimilation_(rActionJson, action);
    }
    else
    {
        RejectInciteKeys_(rActionJson, action.name);
    }
    ParseActionNumbers_(rActionJson, action);
    if (rActionJson.contains("cost") && !rActionJson.at("cost").is_null())
    {
        action.cost = ParseCost_(rActionJson.at("cost"), action.name);
    }
    action.effects = ParseActionEffects_(rActionJson, action.id);
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
    if (!root.contains("success_formula") || !root.at("success_formula").is_object())
    {
        throw std::runtime_error("probe_actions.json: missing required 'success_formula' object");
    }
    const json& rOdds = root.at("success_formula");
    const std::string oddsContext = "probe_actions.json success_formula";
    config.successFormula.defense = RequireFormula_(rOdds, "defense", oddsContext);
    config.successFormula.missionStrength =
        RequireFormula_(rOdds, "mission_strength", oddsContext);
    config.successFormula.escapeStrength = RequireFormula_(rOdds, "escape_strength", oddsContext);

    if (!root.contains("frame_risk") || !root.at("frame_risk").is_number_integer())
    {
        throw std::runtime_error("probe_actions.json: missing required integer 'frame_risk'");
    }
    config.frameRisk = root.at("frame_risk").get<int>();
    if (config.frameRisk < 0)
    {
        throw std::runtime_error("probe_actions.json: frame_risk must be >= 0");
    }

    if (!root.contains("effects"))
    {
        throw std::runtime_error("probe_actions.json: missing required 'effects' array");
    }
    config.effects = EffectConfigParser::ParseEffects(root, EffectSourceKind_t::ProbeAction,
                                                      "probe_actions");

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
