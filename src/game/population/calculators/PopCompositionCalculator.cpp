#include "game/population/calculators/PopCompositionCalculator.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

PopCompositionCalculator::PopCompositionCalculator(const PopCompositionConfig_t& rConfig,
                                                   LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

const PopCompositionConfig_t& PopCompositionCalculator::GetConfig() const
{
    return m_rConfig;
}

PopCompositionResult_t PopCompositionCalculator::Calculate(const PopCompositionInputs_t& rInputs)
{
    const std::unordered_map<std::string, double> vars = {
        {"psych_output",            static_cast<double>(rInputs.psychOutput)},
        {"faction_talent_modifier", static_cast<double>(rInputs.factionTalentModifier)},
    };

    const int targetTalents = m_rLua.EvalInt(m_rConfig.talentFormula, vars);
    if (targetTalents < 0)
    {
        throw std::runtime_error("Pop composition talent formula ('" + m_rConfig.talentFormula
                                 + "') produced " + std::to_string(targetTalents)
                                 + "; targets must not be negative");
    }

    PopCompositionResult_t result;
    result.targetDrones = rInputs.targetDrones;
    result.targetTalents = targetTalents;
    return result;
}

} // namespace ac
