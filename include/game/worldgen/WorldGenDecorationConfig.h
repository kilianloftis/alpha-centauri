#pragma once

#include "game/worldgen/MapGenerationConfig.h"

#include <optional>
#include <stdexcept>
#include <vector>

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

// Land-side elevation reshape for one ErosiveForces_t level (after the oceanCoverage waterline).
struct ErosiveLevelConfig_t
{
    float landRelief = 1.0f;
    float roughness = 0.0f;
};

// Coefficients for Low / Average / High erosive forces. Session enum selects the row.
struct ErosiveDecorationConfig_t
{
    ErosiveLevelConfig_t low{0.75f, 0.02f};
    ErosiveLevelConfig_t average{1.0f, 0.05f};
    ErosiveLevelConfig_t high{1.25f, 0.1f};
};

inline const ErosiveLevelConfig_t& ErosiveLevelFor(const ErosiveDecorationConfig_t& rConfig,
                                                  ErosiveForces_t forces)
{
    switch (forces)
    {
    case ErosiveForces_t::Low:
        return rConfig.low;
    case ErosiveForces_t::Average:
        return rConfig.average;
    case ErosiveForces_t::High:
        return rConfig.high;
    }
    throw std::invalid_argument("Unknown erosive forces value");
}

// Relative frequencies for Flat / Rolling / Rocky. Parser normalizes to sum to 1.
struct RockinessWeights_t
{
    float flat = 0.55f;
    float rolling = 0.35f;
    float rocky = 0.10f;
};

// One elevation tier: weights for tiles with elevation-above-ocean below max (half-open),
// or the unbounded top tier when max is absent.
struct RockinessBand_t
{
    std::optional<int> maxElevationAboveOceanMeters;
    RockinessWeights_t weights;
};

// Pole-row weights plus an ordered elevation ladder (last band has no max).
struct RockinessDecorationConfig_t
{
    RockinessWeights_t poles{0.0f, 0.0f, 1.0f};
    std::vector<RockinessBand_t> bands{
        RockinessBand_t{0, {0.55f, 0.35f, 0.1f}},
        RockinessBand_t{1000, {0.5f, 0.35f, 0.15f}},
        RockinessBand_t{2000, {0.4f, 0.4f, 0.2f}},
        RockinessBand_t{3000, {0.3f, 0.4f, 0.3f}},
        RockinessBand_t{std::nullopt, {0.2f, 0.35f, 0.45f}},
    };
};

// Aquifer decoration: target fraction of tiles the Aquifer entry can occupy.
struct AquiferDecorationConfig_t
{
    float fraction = 0.02f;
};

// World-gen fungus: coverage of eligible tiles, patch size, and in-patch fill; pole rows may
// also roll. Native life scales coverage via fungus_coverage_multiplier.
struct FungusDecorationConfig_t
{
    float coverage = 0.30f;
    int clusterScaleTiles = 12;
    float fill = 0.85f;
    float poleRowChance = 0.5f;
};

// Frequency-weighted tile bonuses (Nutrients/Minerals/Energy/Monolith, …).
struct TileBonusDecorationConfig_t
{
    // Target fraction of tiles a bonus entry can occupy (CanBuildImprovement).
    float fraction = 0.04f;
};

// Post-elevation terrain decoration (moisture, rockiness, aquifers, fungus, tile bonuses)
// plus erosive land-relief coeffs used during elevation.
struct WorldGenDecorationConfig_t
{
    MoistureDecorationConfig_t moisture;
    ErosiveDecorationConfig_t erosive;
    RockinessDecorationConfig_t rockiness;
    AquiferDecorationConfig_t aquifers;
    FungusDecorationConfig_t fungus;
    TileBonusDecorationConfig_t tileBonuses;
};

} // namespace ac
