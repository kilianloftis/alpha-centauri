#include "game/population/calculators/PopCompositionCalculator.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>

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
    const std::unordered_map<std::string, int> vars = {
        {"base_size",               rInputs.baseSize},
        {"psych_output",            rInputs.psychOutput},
        {"faction_drone_modifier",  rInputs.factionDroneModifier},
        {"faction_talent_modifier", rInputs.factionTalentModifier},
    };

    // A negative target can only come from a formula that computed one, since EvalInt throws
    // rather than returning 0. That is a config error, not something to normalize away.
    const auto evaluate = [&](const std::string& rFormula, const char* what) {
        const int value = m_rLua.EvalInt(rFormula, vars);
        if (value < 0)
        {
            throw std::runtime_error("Pop composition " + std::string(what) + " formula ('"
                                     + rFormula + "') produced " + std::to_string(value)
                                     + "; targets must not be negative");
        }
        return value;
    };

    PopCompositionResult_t result;
    result.targetDrones = evaluate(m_rConfig.droneFormula, "drone");
    result.targetTalents = evaluate(m_rConfig.talentFormula, "talent");
    return result;
}

} // namespace ac
