#include "game/faction/base/population/calculators/GrowthCalculator.h"
#include <algorithm>

namespace ac
{

GrowthCalculator::GrowthCalculator(Signal<>& rOnGrowth, Signal<>& rOnStarvation)
    : m_rOnGrowth(rOnGrowth)
    , m_rOnStarvation(rOnStarvation)
{
}

// TODO: confirm exact nutrient threshold formula from game rules
int GrowthCalculator::ComputeNutrientsRequired(int baseSize, int growthRateModifier)
{
    const int base = baseSize * 10;
    const int modified = base - (base * growthRateModifier / 100);
    return std::max(1, modified);
}

void GrowthCalculator::Accumulate(const GrowthInputs_t& inputs)
{
    const int required = ComputeNutrientsRequired(inputs.baseSize, inputs.growthRateModifier);
    m_nutrientBank += inputs.nutrientsPerTurn;

    if (m_nutrientBank >= required)
    {
        m_nutrientBank = 0;
        m_rOnGrowth.emit();
    }
    else if (m_nutrientBank < 0)
    {
        m_nutrientBank = 0;
        m_rOnStarvation.emit();
    }
}

int GrowthCalculator::GetNutrientBank() const
{
    return m_nutrientBank;
}

} // namespace ac
