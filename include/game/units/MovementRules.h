#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class UnitPositionIndex;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Embarked cargo never projects. Air projectors affect land and sea subjects; sea/land
// projectors affect their own domain only. Air subjects never match those rules;
// IgnoreZoneOfControl also exempts.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// Whether rMover may enter rTile for its chassis domain (air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// Tiles rMover can hold on its own: its domain terrain, or a friendly sea base (a land
// unit garrisons one without a hull). Excludes anything that depends on other units being
// present — see CanEnterTile for boarding / Permission(Enter).
bool CanOccupyTileUnaided(const Unit& rMover, const Tile& rTile);

// Full tile-entry predicate used by stepping, unloading, and attack legality.
// CanOccupyTileUnaided, plus land exceptions that depend on what else is on the tile:
// board a friendly transport (TransportRules), or Permission(Enter) onto a qualifying
// sea-base tile. Neither grants free ocean movement.
bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// True when a same-faction unit already occupies rTile (friend-on-fungus shortcut).
bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// True when rMover's faction has a base centered on rTile.
bool HasFriendlyBase(const Unit& rMover, const Tile& rTile);

// Whether a new unit (or move destination) may occupy rTile under the stacking rule.
// The rule itself lives on UnitPositionIndex (see UnitPositionIndex::SetSingleUnitPerTile) —
// one setting per world, beside the occupancy it constrains, rather than a process-wide global
// that two sessions could not disagree about and a test could leak into the next case.
bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions);

} // namespace ac
