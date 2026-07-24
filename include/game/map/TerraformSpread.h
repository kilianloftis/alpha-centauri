#pragma once

#include <random>

namespace ac
{

class Tile;
class TileEffectsContext;
class WorldMap;

// SMAC alien_fauna forest/kelp growth: MapAreaTiles / (turn/4 + 32) random tile
// samples per turn. turnIndex is years since 2100 (0 on the first playable year).
int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex);

// If rOrigin is Forest (land) or KelpFarm (sea), spread once into the best-scoring
// eligible neighbor. Forest spread clears fungus on the destination. Returns true
// if an improvement was placed.
bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects);

// Run one turn of forest/kelp growth using SMAC sampling rates.
void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng);

} // namespace ac
