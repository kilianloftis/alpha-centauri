#include "game/probes/ProbeCalculator.h"

#include "game/probes/ProbeActionConfig.h"
#include "lib/LuaRuntime.h"

#include <string>
#include <unordered_map>

namespace ac
{

ProbeCalculator::ProbeCalculator(LuaRuntime& rLua)
    : m_rLua(rLua)
{}

int ProbeCalculator::MindControlCost(const ProbeCostConfig_t& rCost,
                                     const MindControlCostInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"garrison_weight", static_cast<double>(rInputs.garrisonWeight)},
        {"garrison_multiplier", static_cast<double>(rInputs.garrisonMultiplier)},
        {"mind_control_total", static_cast<double>(rInputs.mindControlTotal)},
        {"population", static_cast<double>(rInputs.population)},
        {"energy", static_cast<double>(rInputs.energy)},
        {"distance", static_cast<double>(rInputs.distance)},
    };
    return m_rLua.EvalInt(rCost.formula, vars);
}

int ProbeCalculator::SubvertCost(const ProbeCostConfig_t& rCost,
                                 const SubvertCostInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"mineral_cost", static_cast<double>(rInputs.mineralCost)},
        {"energy", static_cast<double>(rInputs.energy)},
        {"distance", static_cast<double>(rInputs.distance)},
    };
    return m_rLua.EvalInt(rCost.formula, vars);
}

int ProbeCalculator::DrainShare(const DrainFormulas_t& rDrain,
                                const DrainShareInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"treasury", static_cast<double>(rInputs.treasury)},
        {"population", static_cast<double>(rInputs.population)},
        {"faction_population", static_cast<double>(rInputs.factionPopulation)},
    };
    return m_rLua.EvalInt(rDrain.share, vars);
}

int ProbeCalculator::DrainFloor(const DrainFormulas_t& rDrain, int share, int morale) const
{
    const std::unordered_map<std::string, double> vars = {
        {"share", static_cast<double>(share)},
        {"morale", static_cast<double>(morale)},
    };
    return m_rLua.EvalInt(rDrain.floor, vars);
}

int ProbeCalculator::DrainAmount(const DrainFormulas_t& rDrain, int floorAmount, int draw) const
{
    const std::unordered_map<std::string, double> vars = {
        {"floor_amount", static_cast<double>(floorAmount)},
        {"draw", static_cast<double>(draw)},
    };
    return m_rLua.EvalInt(rDrain.amount, vars);
}

int ProbeCalculator::PlagueKills(const PlagueFormulas_t& rPlague,
                                 const PlagueKillInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"population", static_cast<double>(rInputs.population)},
        {"defense_techs", static_cast<double>(rInputs.defenseTechs)},
        {"known_defense_techs", static_cast<double>(rInputs.knownDefenseTechs)},
        {"repeated", rInputs.bRepeated ? 1.0 : 0.0},
    };
    return m_rLua.EvalInt(rPlague.kills, vars);
}

int ProbeCalculator::PlagueGarrisonDamageBound(const PlagueFormulas_t& rPlague, int hp) const
{
    const std::unordered_map<std::string, double> vars = {
        {"hp", static_cast<double>(hp)},
    };
    return m_rLua.EvalInt(rPlague.garrisonDamageBound, vars);
}

int ProbeCalculator::PlagueGarrisonDamage(const PlagueFormulas_t& rPlague, int hp, int draw,
                                          bool bRepeated) const
{
    const std::unordered_map<std::string, double> vars = {
        {"hp", static_cast<double>(hp)},
        {"draw", static_cast<double>(draw)},
        {"repeated", bRepeated ? 1.0 : 0.0},
    };
    return m_rLua.EvalInt(rPlague.garrisonDamage, vars);
}

} // namespace ac
