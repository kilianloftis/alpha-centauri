#pragma once

#include "game/base/BaseTypes.h"

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
    // Factions the blast cost something, in the order it reached them, detonator included.
    // AtrocityRules::BlastVictim decides who answers for it.
    std::vector<FactionId_t> baseOwnersDestroyed;
    std::vector<FactionId_t> unitOwnersDestroyed;
};

// Destroys every unit but pSpareUnit, razes bases and removes improvements in the disk, then
// lowers it a level. Terrain stays. Radius <= 0 changes nothing.
ExplosionResult_t ApplyExplosion(Tile& rOrigin, WorldMap& rWorldMap, int radius,
                                 std::mt19937& rRng, GameState& rGameState,
                                 const Unit* pSpareUnit);

} // namespace ac
