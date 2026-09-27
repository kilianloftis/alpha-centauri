#pragma once

#include "game/faction/base/BaseTypes.h"

#include <random>
#include <vector>

namespace ac
{

class GameState;
class Tile;
class Unit;
class WorldMap;

struct ExplosionResult_t
{
    int radius = 0;
    int tiles = 0;
    bool bChanged = false;
    // Who the blast cost something, in the order the disk reached them, with no faction named
    // twice. Includes the detonator's own losses: which of these answers for the blast is
    // AtrocityRules::BlastVictim's decision, not this one's.
    std::vector<FactionId_t> baseOwnersDestroyed;
    std::vector<FactionId_t> unitOwnersDestroyed;
};

// Chebyshev disk of `radius` around rOrigin, including the origin. Radius <= 0 changes
// nothing. On each tile: destroy every unit except pSpareUnit (cargo that disembarks is
// destroyed on the next pass), raze a base, and remove built improvements. Optional terrain,
// aquifers, rockiness, and moisture stay. Then every tile in the disk drops one level via
// LowerTilesOneLevel. bChanged is set when any unit, base, improvement, or elevation changed.
ExplosionResult_t ApplyExplosion(Tile& rOrigin, WorldMap& rWorldMap, int radius,
                                 std::mt19937& rRng, GameState& rGameState,
                                 const Unit* pSpareUnit);

} // namespace ac
