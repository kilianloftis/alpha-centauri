#pragma once

namespace ac
{

struct BaseEffects_t;
struct GrowthConfig_t;

// The nutrient box a base must fill to grow one pop: (size + 1) * nutrientsPerPop scaled by
// GrowthRate, seeded at 100. A GrowthRate at or below 0 blocks growth (INT_MAX).
int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize,
                             const BaseEffects_t& rBaseEffects);

} // namespace ac
