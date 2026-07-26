#pragma once

#include "game/map/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class WorldMap;

// Place xenofungus patches on land/water according to decoration knobs.
// Each patch grows from a seed via random orthogonal frontier expansion.
void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig, std::mt19937& rRng);

} // namespace ac
