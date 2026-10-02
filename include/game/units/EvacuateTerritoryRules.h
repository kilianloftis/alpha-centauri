#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class TerritoryMap;
struct InteractionGridsConfig_t;

// True when rTile is owned by hostTerritoryOwner (not unowned, not another faction).
bool IsOnTerritoryOf(const Tile& rTile, FactionId_t hostTerritoryOwner,
                     const TerritoryMap& rTerritory);

// Nearest tile owned by rUnit's faction that rUnit can hold without a carrier, that accepts
// placement under the world's stacking rule, and that holds no unit rUnit may not share a tile
// with (its own tile excepted: a unit at home stays). Expands Chebyshev rings (X wraps) and
// stops at the first ring with a valid tile. Same-ring ties: lowest Y, then lowest X.
// Returns nullptr when none exist.
const Tile* FindNearestOwnTerritoryTile(const Unit& rUnit, const WorldMap& rWorldMap,
                                        const InteractionGridsConfig_t& rGrids);

} // namespace ac
