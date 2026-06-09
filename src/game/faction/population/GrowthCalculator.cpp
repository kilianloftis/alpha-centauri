#include "game/faction/population/GrowthCalculator.h"
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

void GrowthCalculator::Accumulate(const GrowthInputs_t& inputs)
{
    const int required = ComputeNutrientsRequired(inputs.baseSize, inputs.growthRateModifier);
    m_nutrientBank += inputs.nutrientsPerTurn;

    if (m_nutrientBank >= required)
    {
        m_nutrientBank = 0;
        on_growth.emit();
    }
    else if (m_nutrientBank < 0)
    {
        m_nutrientBank = 0;
        on_starvation.emit();
    }
}

int GrowthCalculator::GetNutrientBank() const
{
    return m_nutrientBank;
}

} // namespace ac
