#pragma once

namespace ac
{

// Moisture climate knobs for world generation (SMAC world_rainfall / alphax #WORLDBUILDER).
// Parser requires every key; struct defaults match vanilla alphax for in-code fixtures.
struct MoistureDecorationConfig_t
{
    int cloudmassPeaks = 5;
    int cloudmassHills = 3;
    int rainfallCoeff = 1;
    // Meters above ocean: ALT_TWO_ABOVE_SEA / ALT_THREE_ABOVE_SEA trap thresholds.
    int hillMinElevationMeters = 2000;
    int peakMinElevationMeters = 3000;
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

// Aquifer decoration: target fraction of tiles the Aquifer entry can occupy.
struct AquiferDecorationConfig_t
{
    float fraction = 0.02f;
};

// Fungus decoration: orthogonal patch growth from single tiles to large swaths.
// Target size is drawn from [minPatchTiles, maxPatchTiles] with a power skew:
// size = min + floor((max-min+1) * u^patchSizeSkew). skew 1 = uniform; higher → smaller.
struct FungusDecorationConfig_t
{
    // Target fraction of tiles the fungus entry can occupy (CanBuildImprovement).
    float fraction = 0.08f;
    int minPatchTiles = 1;
    int maxPatchTiles = 16;
    float patchSizeSkew = 4.0f;   // >= 1; higher weights the small end of the range
};

// Frequency-weighted tile bonuses (Nutrients/Minerals/Energy/Monolith, …).
struct TileBonusDecorationConfig_t
{
    // Target fraction of tiles a bonus entry can occupy (CanBuildImprovement).
    float fraction = 0.04f;
};

// Post-elevation terrain decoration (moisture, rockiness, aquifers, fungus, …).
struct WorldGenDecorationConfig_t
{
    MoistureDecorationConfig_t moisture;
    RockinessDecorationConfig_t rockiness;
    AquiferDecorationConfig_t aquifers;
    FungusDecorationConfig_t fungus;
    TileBonusDecorationConfig_t tileBonuses;
};

} // namespace ac
