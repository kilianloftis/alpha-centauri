#pragma once

namespace ac
{

class Tile;
class Unit;
class UnitPositionIndex;
struct EntryTerms_t;

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Embarked cargo never projects and same-faction never applies; both short-circuit here.
// Everything else is the zoc grid, resolved with rSubject as the acting unit — the row is
// the held unit's domain, the column the projector's, so a unit that ignores ZOC declares a
// zoc `deny` on itself (non-default where stock would hold it).
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// Whether rMover may enter rTile for its chassis domain per the enter interaction grid
// (stock: air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// Whether rMover could stand on rTile with no hull under it: enter-grid allow, or a tile
// that TileHarbors the mover's domain for its faction. NOT a movement predicate — it never
// grants entry, and reaching a tile is strictly harder than holding one (a land unit may
// hold its own sea base but may only *reach* it by transport or Amphibious Pods; see
// CanEnterTile). Asked only when a carrier is about to stop supporting a passenger:
// auto-boarding, carrier loss, unload-in-place.
bool CanHoldTileWithoutCarrier(const Unit& rMover, const Tile& rTile);

// Physical tile-entry predicate, used by attack legality.
// Resolve(enter) allow, plus lifts that depend on the tile: sea on land via TileHarbors,
// or land on water via FindBoardableTransport. Neither grants free ocean movement.
bool CanPhysicallyEnterTile(const Unit& rMover, const Tile& rTile);

// Full tile-entry predicate used by stepping, unloading, and airdrops: physical entry, and
// the territory owner's status with rMover's faction allows entering its territory.
bool CanEnterTile(const Unit& rMover, const Tile& rTile);

// True when a unit of rMover's faction, or of a faction it may share tiles with, already
// occupies rTile.
bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile);

// True when rTile holds a base of rMover's faction or of a faction it may share tiles with.
// Used by ZOC callers (StepEvaluator); enter/hold use TileHarbors instead.
bool HasFriendlyBase(const Unit& rMover, const Tile& rTile);

// Whether a new unit (or move destination) may occupy rTile under the stacking rule.
// The rule itself lives on UnitPositionIndex (see UnitPositionIndex::SetSingleUnitPerTile) —
// one setting per world, beside the occupancy it constrains, rather than a process-wide global
// that two sessions could not disagree about and a test could leak into the next case.
bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions);

struct EntryPayment_t
{
    bool bEnters = false;
    // Charged against this turn's balance, which clamps at 0 (fuel is billed on the charge).
    int chargedFragments = 0;
    // Banked toward the tile afterwards; 0 once the unit enters.
    int paidAfter = 0;
};

// A full-cost entry banks every fragment, across turns, until paidSoFar covers the price.
// Otherwise any balance admits the unit; an entry that ends the turn spends all of it.
EntryPayment_t ResolveEntryPayment(const EntryTerms_t& rTerms, int available, int paidSoFar);

} // namespace ac
