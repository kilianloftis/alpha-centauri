#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include "lib/LuaRuntime.h"

namespace ac
{

PopCompositionCalculator::PopCompositionCalculator(const PopCompositionConfig& rConfig,
                                                   LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

const PopCompositionConfig& PopCompositionCalculator::GetConfig() const
{
    return *m_pConfig;
}

PopCompositionResult PopCompositionCalculator::Calculate(const PopCompositionInputs& inputs)
{
    std::unordered_map<std::string, int> vars = {
        {"base_size",               inputs.baseSize},
        {"psych_output",            inputs.psychOutput},
        {"faction_drone_modifier",  inputs.factionDroneModifier},
        {"faction_talent_modifier", inputs.factionTalentModifier},
    };

    PopCompositionResult result;
    result.targetDrones  = m_pLua->EvalInt(m_pConfig->droneFormula,  vars);
    result.targetTalents = m_pLua->EvalInt(m_pConfig->talentFormula, vars);

    if (result.targetDrones  < 0) result.targetDrones  = 0;
    if (result.targetTalents < 0) result.targetTalents = 0;

    return result;
}

} // namespace ac
