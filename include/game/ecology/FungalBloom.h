#pragma once

#include "lib/Rational.h"

#include <random>
#include <span>
#include <string>

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
// then native lifeforms on the new tiles. Throws without a native-life faction. `destroys`
// names the occupants the bloom razes on each newly fungused tile (the FungalBloom effect's
// `destroys`, already tag-expanded); empty destroys nothing.
FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                     std::span<const std::string> destroys, std::mt19937& rRng,
                                     GameState& rGameState);

// Same fungus + lifeform path; rChances[d] rolls independently for Chebyshev distance d.
FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap,
                                     std::span<const Rational_t> rChances,
                                     std::span<const std::string> destroys, std::mt19937& rRng,
                                     GameState& rGameState);

} // namespace ac
