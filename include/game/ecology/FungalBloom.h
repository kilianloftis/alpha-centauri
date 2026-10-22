#pragma once

#include <random>

namespace ac
{

class GameState;
class Tile;
class WorldMap;

struct FungalBloomResult_t
{
    int tilesFungused = 0;
    int lifeforms = 0;
};

// Fungus on the origin (unless a base) and a random sample of non-base neighbours without it,
// then native lifeforms on the new tiles. Throws without a native-life faction.
FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                     std::mt19937& rRng, GameState& rGameState);

} // namespace ac
