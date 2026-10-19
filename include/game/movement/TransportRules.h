#pragma once

#include "game/base/BaseTypes.h"

#include <unordered_set>

namespace ac
{

class Unit;
class Tile;
enum class UnitDomain_t;

// Cargo capability and boarding/unload legality. Tile entry itself lives in MovementRules
// (CanEnterTile), which calls FindBoardableTransport for the boarding case.

// Effective domains the carrier may carry (union of TransportParams::carries; land-only
// default when CargoCapacity > 0 but no carries saw).
std::unordered_set<UnitDomain_t> ResolveCarriedDomains(const Unit& rCarrier);

// Same faction as factionId, and ResolveCarriedDomains contains domain. No capacity check.
bool UnitCarries(const Unit& rCarrier, UnitDomain_t domain, FactionId_t factionId);

bool HasCargoCapacity(const Unit& rCarrier);
int FreeCargoSlots(const Unit& rCarrier);

bool CanCarryPassenger(const Unit& rCarrier, const Unit& rPassenger);

// True when the carrier requires no harbor, or rTile harbors the carrier's domain for its
// faction (TileHarbors). requires_harbor ORs across every TransportParams on the carrier.
bool CanLoadAtTile(const Unit& rCarrier, const Tile& rTile);

// First non-embarked friendly carrier on rTile that can accept rPassenger.
// Used by MovementRules::CanEnterTile and by explicit attach orders.
Unit* FindBoardableTransport(const Unit& rPassenger, const Tile& rTile);

// Whether an embarked passenger may step off its carrier's tile onto rTo
// (adjacency + MovementRules::CanPhysicallyEnterTile; territory is checked by the step).
bool CanUnloadTo(const Unit& rPassenger, const Tile& rFrom, const Tile& rTo);

// Whether rPassenger survives its carrier being destroyed on rTile: cargo that can hold the
// tile unaided is set down there, anything else goes down with the carrier.
bool SurvivesCarrierLoss(const Unit& rPassenger, const Tile& rTile);

// Drop all cargo of an air carrier onto its current tile when every passenger can hold it
// unaided.
bool CanUnloadTransportInPlace(const Unit& rCarrier);

} // namespace ac
