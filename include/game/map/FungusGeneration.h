#pragma once

#include "game/map/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class WorldMap;
struct ImprovementConfig_t;

// Place xenofungus patches on the fraction of tiles the fungus entry can occupy.
// Each patch grows from a seed via random orthogonal frontier expansion.
void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig,
                 const ImprovementConfig_t& rFungus, std::mt19937& rRng);

} // namespace ac
