#pragma once

#include "game/map/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class ImprovementRegistry;
class WorldMap;

// Place frequency-weighted tile bonuses (Nutrients, Minerals, Energy, Monolith, …)
// on land. Improvements with frequency <= 0 are skipped. Relative frequencies act
// as selection weights among candidates that can coexist on a given tile.
int PlaceTileBonuses(WorldMap& rWorld,
                     const TileBonusDecorationConfig_t& rConfig,
                     const ImprovementRegistry& rImprovements,
                     std::mt19937& rRng);

} // namespace ac
