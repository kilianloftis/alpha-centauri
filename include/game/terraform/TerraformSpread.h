#pragma once

#include <random>

namespace ac
{

class Tile;
class TileEffectsContext;
class WorldMap;

// If rOrigin is Forest (land) or KelpFarm (sea), spread once into the best-scoring
// eligible neighbor. Forest spread clears fungus on the destination. Returns true
// if an improvement was placed.
bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects);

// Run one turn of forest/kelp growth using SMAC sampling rates.
void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng);

} // namespace ac
