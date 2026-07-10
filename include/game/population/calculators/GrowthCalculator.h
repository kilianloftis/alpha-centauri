#pragma once

namespace ac
{

struct BaseEffects_t;
struct GrowthConfig_t;

// Calculates the nutrient threshold required for a base to grow one population.
// Formula: baseSize * nutrientsPerPop / (GrowthRate/100), with GrowthRate seeded at 100.
// GrowthRate ≤ 0 blocks threshold growth (returns INT_MAX); NearZeroGrowth / PopulationBoom
// rule flags are not consumed yet (TODO).
class GrowthCalculator
{
public:
    GrowthCalculator() = delete;

    // Returns the nutrient threshold required to grow given the current base size.
    // GrowthRate stat modifiers in rBaseEffects are resolved internally.
    static int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize, const BaseEffects_t& rBaseEffects);
};

} // namespace ac
