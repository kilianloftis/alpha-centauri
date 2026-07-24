#pragma once

namespace ac
{

// Moisture climate knobs for world generation (coastal / tropical / orographic).
struct MoistureDecorationConfig_t
{
    float baseMin = 0.25f;
    float baseRange = 0.50f;

    float coastalPeakBonus = 0.12f;
    int coastalRadius = 2;

    float tropicalPeakBonus = 0.10f;
    float tropicalHalfWidth = 0.35f;

    float orographicStrength = 0.45f;
    float orographicElevScale = 1000.0f;
    float orographicMaxElev = 4000.0f;

    float aridThreshold = 0.40f;
    float moistThreshold = 0.70f;
};

// Post-elevation terrain decoration (moisture today; rockiness/etc. can join later).
struct WorldGenDecorationConfig_t
{
    MoistureDecorationConfig_t moisture;
};

} // namespace ac
