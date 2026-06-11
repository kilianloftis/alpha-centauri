#include "game/faction/base/population/calculators/GrowthCalculator.h"
#include <algorithm>

namespace ac
{

// TODO: confirm exact nutrient threshold formula from game rules
int GrowthCalculator::ComputeNutrientsRequired(int baseSize, int growthRateModifier)
{
    const int base = baseSize * 10;
    const int modified = base - (base * growthRateModifier / 100);
    return std::max(1, modified);
}

GrowthResult GrowthCalculator::CalculateGrowh(const GrowthInputs_t& inputs, int& outNewBank)
{
    const int required = ComputeNutrientsRequired(inputs.baseSize, inputs.growthRateModifier);
    outNewBank = inputs.nutrientBank;

    if (outNewBank >= required)
    {
        outNewBank = 0;
        return GrowthResult::Growth;
    }
    else if (outNewBank < 0)
    {
        outNewBank = 0;
        return GrowthResult::Starvation;
    }

    return GrowthResult::None;
}

} // namespace ac
