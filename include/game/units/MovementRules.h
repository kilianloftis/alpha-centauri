#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Flight projectors affect land and sea subjects; sea/land projectors affect their own
// domain only. Subjects that IgnoresZoneOfControl() are never affected.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// True if rTile is in hostile ZOC for rMover (Chebyshev-1 around a qualifying foreign unit).
bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// True when moving from rFrom to rTo would illegally transit through hostile ZOC
// (ZOC -> ZOC). Attack is not a move — see TryAttack.
bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                    const WorldMap& rWorldMap);

// Destination has at least one unit from another faction.
bool HasHostileUnit(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// Whether rMover may enter rTile for its domain (flight any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// Adjacent step legality: moves remaining, adjacency, terrain, stacking, ZOC.
// Hostile-occupied tiles are never enterable (combat is adjacent; see TryAttack).
bool CanStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap);

// Combat placeholder: no damage yet. Clears remaining moves and any active order.
void ResolveCombatStub(Unit& rAttacker);

// Attack a hostile-occupied adjacent tile without moving onto it. Returns false if not
// adjacent, out of moves, or rTargetTile has no hostile unit.
bool TryAttack(Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap);

// Apply one legal empty-tile step (TryMoveUnit + spend 1 move). Returns false if CanStep
// fails or the occupancy move is rejected.
bool TryStep(Unit& rMover, const Tile& rTo, WorldMap& rWorldMap);

// Temporary next-step seam (real pathfinding later). Among adjacent tiles that pass
// CanStep, pick the one that minimizes Chebyshev distance to rDestination. Recalculate
// every call. Returns nullptr if already there or no improving legal step exists.
const Tile* ProposeNextStep(const Unit& rMover, const Tile& rDestination,
                            const WorldMap& rWorldMap);

} // namespace ac
