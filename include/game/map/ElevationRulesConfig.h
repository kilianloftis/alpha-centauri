#pragma once

namespace ac
{

// config/map_rules.json — planet elevation domain and play-time elevation edits.
// Yield energy uses tile_yield_rules.json and is not this file.
struct ElevationRulesConfig_t
{
    // Inclusive storage range. SetElevation rejects meters outside it.
    int minElevationMeters = 0;
    int maxElevationMeters = 0;
    // Elevation below this is water. Land lower stops at this line.
    int oceanLevelMeters = 0;
    // Water at or above this, and below ocean level, is the shelf. Deeper water is ocean.
    int oceanShelfMeters = 0;
    // Inclusive meter range of one level. A level is one uniform draw in this range.
    int levelMinMeters = 0;
    int levelMaxMeters = 0;
    // Chebyshev neighbors of a tile this edit changed are pulled until the gap is at most this.
    int maxAdjacentDifferenceMeters = 0;
    // Former eligibility and the raise/lower energy band. Not the rolled level size.
    int referenceLevelMeters = 0;
    // Land above this only takes forest/fungus spread when it is moist or wet. Ships the same
    // number as reference_level_meters, but one is a spread rule and the other a terraform
    // rule, and a mod may want to move either alone.
    int spreadAltitudeLimitMeters = 0;
};

} // namespace ac
