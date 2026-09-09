#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class UnitPositionIndex;
struct InteractionGridsConfig_t;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Embarked cargo never projects. Stock pairs come from the zoc interaction grid; IgnoreZOC
// and same-faction still short-circuit here.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject,
                     const InteractionGridsConfig_t& rGrids);

// Whether rMover may enter rTile for its chassis domain per the enter interaction grid
// (stock: air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile,
                         const InteractionGridsConfig_t& rGrids);

// Tiles rMover can hold on its own: enter-grid allow, or a friendly sea base (a land unit
// garrisons one without a hull). Excludes anything that depends on other units being
// present — see CanEnterTile for boarding / InteractionOverride enter.
bool CanOccupyTileUnaided(const Unit& rMover, const Tile& rTile,
                          const InteractionGridsConfig_t& rGrids);

// Full tile-entry predicate used by stepping, unloading, and attack legality.
// Resolve(enter) allow, plus land exceptions that depend on what else is on the tile:
// board a friendly transport, or CanOccupyTileUnaided (friendly sea base). Neither grants
// free ocean movement.
bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                  const InteractionGridsConfig_t& rGrids);

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
