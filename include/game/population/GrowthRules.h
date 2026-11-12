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
    bool bPopulationBoom = false;
    bool bNearZeroGrowth = false;
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

// Boom needs net surplus of at least one citizen's intake (intake 0 disables boom).
bool MeetsPopulationBoomSurplus(int netNutrients, const GrowthConfig_t& rConfig);

// Rewrites tank state from this turn's net without depositing nutrients: lift a negative
// stockpile to 0 when net >= 0; set an empty stockpile to -1 when net < 0. Must run after
// ResourceCollection and before BaseProduction so a colony-pod pop cost can clear -1 to 0
// before removing citizens (docs/thinker/nutrient-growth.md).
int PrimeNutrientTanks(int stockpile, int netNutrients);

GrowthOutcome_t ResolveGrowth(const GrowthInputs_t& rInputs, const GrowthConfig_t& rConfig);

} // namespace ac
