#pragma once

#include "game/map/SmacFractal.h"
#include "game/worldgen/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class WorldMap;
struct ImprovementConfig_t;

// Place xenofungus from fractal bands, widened by the session's native life setting
// (docs/thinker/smac-world-generation.md).
void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig, int bandWidening,
                 const ImprovementConfig_t& rFungus, const SmacFractal& rFractal,
                 std::mt19937& rRng);

} // namespace ac
