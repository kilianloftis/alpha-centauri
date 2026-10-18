#pragma once

namespace ac
{

struct BaseEffects_t;
struct GrowthConfig_t;

// The nutrient box a base must fill to grow one pop: (size + 1) * nutrientsPerPop scaled by
// GrowthRate, seeded at 100. A GrowthRate at or below 0 blocks growth (INT_MAX).
int ComputeNutrientsRequired(const GrowthConfig_t& rConfig, int baseSize,
                             const BaseEffects_t& rBaseEffects);

struct GrowthInputs_t
{
    int stockpile = 0;
    // Gross production minus the current citizens' intake.
    int netNutrients = 0;
    int boxNow = 0;
    int boxNext = 0;
    bool bRoomToGrow = false;
};

struct GrowthOutcome_t
{
    bool bGrows = false;
    bool bStarves = false;
    // What a growing base keeps before this turn's deposit.
    int stockpileOnGrowth = 0;
    // After this turn's deposit.
    int stockpileAfter = 0;
};

GrowthOutcome_t ResolveGrowth(const GrowthInputs_t& rInputs, const GrowthConfig_t& rConfig);

} // namespace ac
