#include "game/probes/ProbeActionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <magic_enum.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

ProbeTargetKind_t ParseTarget_(const ConfigReader& rReader)
{
    const std::string target = rReader.String("target");
    if (target == "base")
    {
        return ProbeTargetKind_t::Base;
    }
    if (target == "unit")
    {
        return ProbeTargetKind_t::Unit;
    }
    rReader.FailKey("target", "is not a known probe target: '" + target + "'");
}

void RejectForeignKey_(const ConfigReader& rReader, const std::string& rKey, ProbeActionId_t owner)
{
    if (rReader.Has(rKey))
    {
        rReader.FailKey(rKey, "applies only to " + std::string(magic_enum::enum_name(owner)));
    }
}

// An integer one action's rule reads: required (at least `minimum`) on that action, rejected on
// any other, which would ignore it.
int ParseActionInt_(const ConfigReader& rReader, const ProbeActionConfig_t& rAction,
                    ProbeActionId_t owner, const std::string& rKey, int minimum)
{
    if (rAction.id != owner)
    {
        RejectForeignKey_(rReader, rKey, owner);
        return 0;
    }
    return rReader.IntAtLeast(rKey, minimum);
}

// Formulas one action's rules read: rejected on any other, which would ignore them.
bool OwnsKeys_(const ConfigReader& rReader, const ProbeActionConfig_t& rAction,
               ProbeActionId_t owner, const std::vector<std::string>& rKeys)
{
    if (rAction.id == owner)
    {
        return true;
    }
    for (const std::string& rKey : rKeys)
    {
        RejectForeignKey_(rReader, rKey, owner);
    }
    return false;
}

std::optional<DrainFormulas_t> ParseDrainFormulas_(const ConfigReader& rReader,
                                                   const ProbeActionConfig_t& rAction)
{
    if (!OwnsKeys_(rReader, rAction, ProbeActionId_t::DrainEnergy,
                   {"share_formula", "floor_formula", "amount_formula"}))
    {
        return std::nullopt;
    }
    return DrainFormulas_t{rReader.NonEmptyString("share_formula"),
                           rReader.NonEmptyString("floor_formula"),
                           rReader.NonEmptyString("amount_formula")};
}

std::optional<PlagueFormulas_t> ParsePlagueFormulas_(const ConfigReader& rReader,
                                                     const ProbeActionConfig_t& rAction)
{
    if (!OwnsKeys_(rReader, rAction, ProbeActionId_t::GeneticPlague,
                   {"kill_formula", "garrison_damage_bound_formula", "garrison_damage_formula"}))
    {
        return std::nullopt;
    }
    return PlagueFormulas_t{rReader.NonEmptyString("kill_formula"),
                            rReader.NonEmptyString("garrison_damage_bound_formula"),
                            rReader.NonEmptyString("garrison_damage_formula")};
}

void ParseActionNumbers_(const ConfigReader& rReader, ProbeActionConfig_t& rAction)
{
    rAction.protectedRisk = ParseActionInt_(rReader, rAction, ProbeActionId_t::SabotageFacility,
                                            "protected_risk", 0);
    rAction.drain = ParseDrainFormulas_(rReader, rAction);
    rAction.plague = ParsePlagueFormulas_(rReader, rAction);
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

void ParseInciteAssimilation_(const ConfigReader& rReader, ProbeActionConfig_t& rAction)
{
    rAction.assimilationTurnsAdded = rReader.IntAtLeast("assimilation_turns_added", 1);
    rAction.assimilationTurnsCapFactor = rReader.NumberAbove("assimilation_turns_cap_factor", 0.0);
}

void RejectInciteKeys_(const ConfigReader& rReader)
{
    RejectForeignKey_(rReader, "assimilation_turns_added", ProbeActionId_t::InciteDroneRiots);
    RejectForeignKey_(rReader, "assimilation_turns_cap_factor",
                      ProbeActionId_t::InciteDroneRiots);
}

std::vector<EffectConfig_t> ParseActionEffects_(const ConfigReader& rReader, ProbeActionId_t id)
{
    if (!rReader.Has("effects"))
    {
        return {};
    }
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rReader.Json().at("effects");
    const std::string sourceId(magic_enum::enum_name(id));
    return EffectConfigParser::ParseEffects(wrapper, EffectSourceKind_t::ProbeAction, sourceId);
}

bool HasNonNull_(const ConfigReader& rReader, const char* pKey)
{
    return rReader.Has(pKey) && !rReader.Json().at(pKey).is_null();
}

ProbeActionConfig_t ParseAction_(const ConfigReader& rReader)
{
    ProbeActionConfig_t action;
    const std::string idName = rReader.String("id");
    action.id = rReader.Enum<ProbeActionId_t>("id", "probe action id");
    action.name = rReader.StringOr("name", idName);
    action.target = ParseTarget_(rReader);
    action.bMayKeepAgreement = rReader.Bool("may_keep_agreement");
    action.bHostileActOnBreak = rReader.Bool("hostile_act_on_break");
    action.bMayFrame = rReader.Bool("may_frame");
    action.risk = rReader.IntOr("risk", 0);
    if (HasNonNull_(rReader, "risk_repeat"))
    {
        action.riskRepeat = rReader.Int("risk_repeat");
    }
    action.requiredTech = rReader.StringOr("required_tech", "");
    action.bHqOnly = rReader.BoolOr("hq_only", false);
    action.bNotHq = rReader.BoolOr("not_hq", false);
    if (action.id == ProbeActionId_t::InciteDroneRiots)
    {
        ParseInciteAssimilation_(rReader, action);
    }
    else
    {
        RejectInciteKeys_(rReader);
    }
    ParseActionNumbers_(rReader, action);
    if (HasNonNull_(rReader, "cost"))
    {
        action.cost = ProbeCostConfig_t{rReader.Object("cost").NonEmptyString("formula")};
    }
    action.effects = ParseActionEffects_(rReader, action.id);
    const std::string sourceId(magic_enum::enum_name(action.id));
    if (rReader.Has("on_success_effects") && !RunsOnSuccessEffects_(action.id))
    {
        rReader.FailKey("on_success_effects", "is not run by this action's handler");
    }
    action.onSuccessEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_success_effects", sourceId);
    if (rReader.Has("on_paid_effects") && !action.cost.has_value())
    {
        rReader.FailKey("on_paid_effects", "requires a cost");
    }
    action.onPaidEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_paid_effects", sourceId);
    return action;
}

ProbeSuccessFormula_t ParseSuccessFormula_(const ConfigReader& rReader)
{
    ProbeSuccessFormula_t formula;
    formula.defense = rReader.NonEmptyString("defense");
    formula.missionStrength = rReader.NonEmptyString("mission_strength");
    formula.escapeStrength = rReader.NonEmptyString("escape_strength");
    return formula;
}

ProbeActionsConfig_t ParseProbeActions_(const ConfigReader& rReader)
{
    ProbeActionsConfig_t config;
    config.successFormula = ParseSuccessFormula_(rReader.Object("success_formula"));
    config.frameRisk = rReader.IntAtLeast("frame_risk", 0);
    rReader.Require("effects");
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::ProbeAction, "probe_actions");
    rReader.NonEmptyArray("actions");
    for (const ConfigReader& rActionReader : rReader.Objects("actions"))
    {
        config.actions.push_back(ParseAction_(rActionReader));
    }
    return config;
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
    return JsonConfigLoader::LoadObjectFile<ProbeActionsConfig_t>(configPath, "probe actions",
                                                                  ParseProbeActions_);
}

} // namespace ac
