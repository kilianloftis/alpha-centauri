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

// True when rUnit stands where MayEnterTerritoryOf is now false (e.g. Covert lost on foreign land).
bool MustVacateCurrentTerritory(const Unit& rUnit);

// When true, territory redraw and Covert/identity reveal both relocate units that
// MustVacateCurrentTerritory. Shipping is false (SMAC: only pact_withdraw evacuates).
// Future switch EvacuateUnitsOnTerritoryChange reads here without changing the signature.
bool EvacuateUnitsOnTerritoryChange();

// rUnit stands in one of rHost's bases or on a tile with one of rHost's units.
bool SharesTileWith(const Unit& rUnit, const Faction& rHost);

// Nearest own-territory tile rUnit can hold, place and share, by Chebyshev ring with ties to
// lowest Y then X. A unit already at home stays; nullptr when none.
const Tile* FindNearestOwnTerritoryTile(const Unit& rUnit);

} // namespace ac
