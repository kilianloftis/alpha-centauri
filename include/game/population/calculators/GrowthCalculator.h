#pragma once

#include <vector>

namespace ac
{

struct ActiveEffect_t;
struct GrowthConfig_t;

// Calculates the nutrient threshold required for a base to grow one population.
// Formula: baseSize * nutrientsPerPop (configured in pop_growth.json).
class GrowthCalculator
{
public:
    GrowthCalculator() = delete;

    // Returns the nutrient threshold required to grow given the current base size.
    // GrowthRate stat modifiers in activeEffects are resolved internally.
    static int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize, const std::vector<ActiveEffect_t>& activeEffects);
};

} // namespace ac
