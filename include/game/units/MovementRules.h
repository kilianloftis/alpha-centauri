#pragma once

namespace ac
{

class Tile;
class Unit;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Air projectors affect land and sea subjects; sea/land projectors affect their own
// domain only. Air subjects never match those rules; IgnoreZoneOfControl also exempts.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// Whether rMover may enter rTile for its chassis domain (air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

} // namespace ac
