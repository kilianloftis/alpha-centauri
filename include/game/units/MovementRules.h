#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class UnitPositionIndex;
struct InteractionGridsConfig_t;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Embarked cargo never projects and same-faction never applies; both short-circuit here.
// Everything else is the zoc grid, resolved with rSubject as the acting unit — the row is
// the held unit's domain, the column the projector's, so a unit that ignores ZOC declares a
// zoc `deny` on itself (non-default where stock would hold it).
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject,
                     const InteractionGridsConfig_t& rGrids);

// Whether rMover may enter rTile for its chassis domain per the enter interaction grid
// (stock: air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile,
                         const InteractionGridsConfig_t& rGrids);

// Whether rMover could stand on rTile with no hull under it: enter-grid allow, or a tile
// that TileHarbors the mover's domain for its faction. NOT a movement predicate — it never
// grants entry, and reaching a tile is strictly harder than holding one (a land unit may
// hold its own sea base but may only *reach* it by transport or Amphibious Pods; see
// CanEnterTile). Asked only when a carrier is about to stop supporting a passenger:
// auto-boarding, carrier loss, unload-in-place.
bool CanHoldTileWithoutCarrier(const Unit& rMover, const Tile& rTile,
                               const WorldMap& rWorldMap,
                               const InteractionGridsConfig_t& rGrids);

// Full tile-entry predicate used by stepping, unloading, and attack legality.
// Resolve(enter) allow, plus lifts that depend on the tile: sea on land via TileHarbors,
// or land on water via FindBoardableTransport. Neither grants free ocean movement.
bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                  const InteractionGridsConfig_t& rGrids);

// True when a same-faction unit already occupies rTile (friend-on-fungus shortcut).
bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// True when rMover's faction has a base centered on rTile. Used by ZOC callers
// (StepEvaluator); enter/hold use TileHarbors instead.
bool HasFriendlyBase(const Unit& rMover, const Tile& rTile);

// Whether a new unit (or move destination) may occupy rTile under the stacking rule.
// The rule itself lives on UnitPositionIndex (see UnitPositionIndex::SetSingleUnitPerTile) —
// one setting per world, beside the occupancy it constrains, rather than a process-wide global
// that two sessions could not disagree about and a test could leak into the next case.
bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions);

} // namespace ac
