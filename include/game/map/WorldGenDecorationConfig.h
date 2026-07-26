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

// Relative frequencies for Flat / Rolling / Rocky at one erosive-forces level.
// Parsed weights are normalized to sum to 1.
struct RockinessWeights_t
{
    float flat = 0.55f;
    float rolling = 0.35f;
    float rocky = 0.10f;
};

// Rockiness decoration: one weight table per ErosiveForces_t.
// Higher erosive forces → flatter (more Flat, less Rocky), matching classic SMAC.
// decoration.json must define every enum value (parser throws otherwise).
struct RockinessDecorationConfig_t
{
    RockinessWeights_t low{0.45f, 0.35f, 0.20f};
    RockinessWeights_t average{0.55f, 0.35f, 0.10f};
    RockinessWeights_t high{0.70f, 0.25f, 0.05f};
};

// Aquifer decoration: target fraction of land tiles that become river sources.
struct AquiferDecorationConfig_t
{
    float landFraction = 0.02f;
};

// Fungus decoration: orthogonal patch growth from single tiles to large swaths.
struct FungusDecorationConfig_t
{
    float landFraction = 0.08f;   // target fraction of land tiles with fungus
    float waterFraction = 0.0f;   // target fraction of water tiles (sea fungus)
    int minPatchTiles = 1;
    int maxPatchTiles = 48;
};

// Post-elevation terrain decoration (moisture, rockiness, aquifers, fungus, …).
struct WorldGenDecorationConfig_t
{
    MoistureDecorationConfig_t moisture;
    RockinessDecorationConfig_t rockiness;
    AquiferDecorationConfig_t aquifers;
    FungusDecorationConfig_t fungus;
};

} // namespace ac
