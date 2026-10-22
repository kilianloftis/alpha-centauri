#pragma once

#include "game/worldgen/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class ImprovementRegistry;
class WorldMap;

// Places tile bonuses on land, weighted by frequency among those that can coexist on each tile;
// frequency <= 0 is skipped.
int PlaceTileBonuses(WorldMap& rWorld,
                     const TileBonusDecorationConfig_t& rConfig,
                     const ImprovementRegistry& rOccupants,
                     std::mt19937& rRng);

} // namespace ac
