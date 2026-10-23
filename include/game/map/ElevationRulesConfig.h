#pragma once

namespace ac
{

// config/map_rules.json: the ocean line and play-time elevation edits. The storage range is
// copied from the world-gen preset.
struct ElevationRulesConfig_t
{
    // Inclusive storage range from the preset that generated this world.
    // SetElevation rejects meters outside it.
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
    // Land above this takes forest and fungus spread only when moist or wet. Separate from
    // reference_level_meters so a mod can move either.
    int spreadAltitudeLimitMeters = 0;
    // A base flooded without a Pressure Dome loses clamp(size / 2, roll, cap) pop, the roll
    // uniform in [rollMin, rollMax] (terranx alt_set: 2-4, capped at 10).
    int submergedPopLossRollMin = 0;
    int submergedPopLossRollMax = 0;
    int submergedPopLossCap = 0;
};

} // namespace ac
