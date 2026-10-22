#pragma once

namespace ac
{

class Tile;
class Unit;
class UnitPositionIndex;
struct EntryTerms_t;

// Whether foreign rProjector's ZOC holds rSubject. The grid resolves with rSubject acting, so
// a unit that ignores ZOC declares a zoc deny on itself.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// Whether rMover may enter rTile for its chassis domain per the enter interaction grid
// (stock: air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// Whether rMover could stand on rTile unsupported. Not a movement predicate: asked only when
// a carrier stops supporting a passenger.
bool CanHoldTileWithoutCarrier(const Unit& rMover, const Tile& rTile);

// Tile entry for attack legality: the enter grid plus harbor and boarding lifts.
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

// Whether a unit may occupy rTile under the world's stacking rule.
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
