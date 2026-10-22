#pragma once

#include "game/base/BaseTypes.h"

namespace ac
{

class Faction;
class Tile;
class Unit;
class TerritoryMap;

// True when rTile is owned by hostTerritoryOwner (not unowned, not another faction).
bool IsOnTerritoryOf(const Tile& rTile, FactionId_t hostTerritoryOwner,
                     const TerritoryMap& rTerritory);

// A guest unit the host evacuates from its territory: on it, and not a covert unit whose owner
// the host cannot identify.
bool MustLeaveTerritory(const Unit& rUnit, FactionId_t hostTerritoryOwner,
                        const TerritoryMap& rTerritory);

// rUnit stands in one of rHost's bases or on a tile with one of rHost's units.
bool SharesTileWith(const Unit& rUnit, const Faction& rHost);

// Nearest own-territory tile rUnit can hold, place and share, by Chebyshev ring with ties to
// lowest Y then X. A unit already at home stays; nullptr when none.
const Tile* FindNearestOwnTerritoryTile(const Unit& rUnit);

} // namespace ac
