#pragma once

#include <random>

namespace ac
{

class Tile;
class TileEffectsContext;
class WorldMap;

// Spread once onto the neighbour PickSpreadTarget chooses. Returns true if an improvement was
// placed.
bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects);

// Run one turn of forest/kelp growth using SMAC sampling rates.
void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng);

} // namespace ac
