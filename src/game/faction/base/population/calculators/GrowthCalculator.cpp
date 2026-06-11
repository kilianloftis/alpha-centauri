#include "game/faction/base/population/calculators/GrowthCalculator.h"
#include "game/faction/base/population/pop-types/GrowthConfigParser.h"
#include "lib/LuaRuntime.h"
#include <algorithm>
#include <unordered_map>

namespace ac
{

GrowthCalculator::GrowthCalculator(const GrowthConfig& rConfig, LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

int GrowthCalculator::ComputeNutrientsRequired(int baseSize, int growthRating) const
{
    const std::unordered_map<std::string, int> vars = {
        {"base_size",    baseSize},
        {"growth_rating", growthRating},
    };
    const int result = m_pLua->EvalInt(m_pConfig->thresholdFormula, vars);
    if (result < 0)
    {
        throw std::runtime_error("Growth threshold formula returned negative value");
    }
    return result;
}

} // namespace ac
