#pragma once

#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenDecorationConfig.h"
#include "game/map/WorldMap.h"

namespace ac
{
namespace moisture_gen
{

// SMAC altitude bands used by world_rainfall (TerrainAltitude).
constexpr int k_AltOceanShelf = 2;
constexpr int k_AltShoreLine = 3;
constexpr int k_AltOneAboveSea = 4;
constexpr int k_AltTwoAboveSea = 5;
constexpr int k_AltThreeAboveSea = 6;
constexpr int k_AltFourAboveSea = 7;

// Mark-pass bitflags (Thinker MAP::unk_1 during world_rainfall).
constexpr int k_FlagLeeDry = 0x10;       // east of peak / rain shadow
constexpr int k_FlagWindwardWet = 0x20;  // west of peak
constexpr int k_FlagOceanMoisture = 0x80;

// Map continuous elevation (meters) onto SMAC altitude bands using decoration hill/peak
// thresholds. Water below oceanLevel maps to shelf-or-deeper (≤ k_AltOceanShelf).
int AltitudeBand(int elevationMeters,
                 int oceanLevelMeters,
                 const MoistureDecorationConfig_t& rConfig);

// Lat-based temperature stub (0..2) so rainfall temp gates still fire. Not a port of
// world_temperature (solar/thermal/orbit/council); do not persist on Tile.
int StubTemperature(int y, int mapHeight);

// Port of Thinker world_rainfall (vanilla 0x5C4470): cloudmass west-wet / east-dry belts,
// ocean moisture blown east, rain-belt row scan, neighbor smooth to moist.
// Call after rivers (and landmarks) so BIT_RIVER / jungle / dunes / unity match SMAC.
// Condenser bits stay in RecomputeMoisture; forest humidity omitted (no world-gen forests).
void GenerateRainfall(WorldMap& rWorld,
                      const MoistureDecorationConfig_t& rConfig,
                      Rainfall_t rainfall);

} // namespace moisture_gen
} // namespace ac
