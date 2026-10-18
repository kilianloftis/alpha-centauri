#include "game/economy/CommerceCalculator.h"

#include "game/economy/CommerceConfig.h"
#include "lib/LuaRuntime.h"

#include <string>
#include <unordered_map>

namespace ac
{

CommerceCalculator::CommerceCalculator(const CommerceConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{}

int CommerceCalculator::PairValue(const CommercePairInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"energy_ours", static_cast<double>(rInputs.energyOurs)},
        {"energy_theirs", static_cast<double>(rInputs.energyTheirs)},
        {"pair_multiplier", m_rConfig.pairMultiplier},
        {"commerce_tech", static_cast<double>(rInputs.commerceTech)},
        {"tech_denominator", static_cast<double>(rInputs.techDenominator)},
    };
    return m_rLua.EvalInt(m_rConfig.formula, vars);
}

} // namespace ac
