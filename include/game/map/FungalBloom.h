#pragma once

#include <random>

namespace ac
{

class GameState;
class NativeUnitRegistry;
class Tile;
class WorldMap;

struct FungalBloomResult_t
{
    int tilesFungused = 0;
    int lifeforms = 0;
};

// Origin is included when it is not a base. The rest of the count is a random sample of
// Chebyshev-1 neighbors that are not bases and do not already have fungus. tileCount <= 0
// changes nothing. Setting fungus notifies the tile, which drops incompatible improvements
// when a listener is bound. A uniform draw from rNatives then spawns native lifeforms on
// the new tiles, owned by the session's native-life faction. A missing native-life faction
// throws when a lifeform would spawn.
FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                     std::mt19937& rRng, GameState& rGameState,
                                     const NativeUnitRegistry& rNatives);

} // namespace ac
