#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class UnitPositionIndex;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Air projectors affect land and sea subjects; sea/land projectors affect their own
// domain only. Air subjects never match those rules; IgnoreZoneOfControl also exempts.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// Whether rMover may enter rTile for its chassis domain (air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// True when a same-faction unit already occupies rTile (friend-on-fungus shortcut).
bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// Stacking rule: when true, at most one unit may occupy a tile; when false (default),
// units stack without limit, as in the original game.
// TODO: static stand-in until a real game-configuration system exists.
void SetSingleUnitPerTile(bool bSingleUnitPerTile);
bool IsSingleUnitPerTile();

// Whether a new unit (or move destination) may occupy rTile under the stacking rule.
bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions);

} // namespace ac
