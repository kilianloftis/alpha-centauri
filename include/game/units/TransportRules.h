#pragma once

#include "game/effects/EffectEnums.h"

#include <unordered_set>

namespace ac
{

class Unit;
class Tile;
class WorldMap;
enum class UnitDomain_t;

// Cargo capability and boarding/unload mutations. Tile entry itself lives in MovementRules
// (CanEnterTile), which calls FindBoardableTransport for the boarding case.

// Effective passenger domains the carrier may take (union of TransportParams; land-only
// default when CargoCapacity > 0 but no params apply).
std::unordered_set<UnitDomain_t> ResolvePassengerDomains(const Unit& rCarrier);

// Union of load_site_flags from the carrier's TransportParams.
std::unordered_set<RuleFlagId_t> ResolveLoadSiteFlags(const Unit& rCarrier);

bool HasCargoCapacity(const Unit& rCarrier);
int FreeCargoSlots(const Unit& rCarrier);

bool CanCarryPassenger(const Unit& rCarrier, const Unit& rPassenger);

// True when the carrier requires no load-site capability, or rTile provides any one it
// requires (from an improvement on the tile or a friendly unit standing there).
bool CanLoadAtTile(const Unit& rCarrier, const Tile& rTile, const WorldMap& rWorldMap);

// First non-embarked friendly carrier on rTile that can accept rPassenger.
// Used by MovementRules::CanEnterTile and by explicit attach orders.
Unit* FindBoardableTransport(const Unit& rPassenger, const Tile& rTile,
                             const WorldMap& rWorldMap);

// Whether an embarked passenger may step off its carrier's tile onto rTo
// (adjacency + MovementRules::CanEnterTile).
bool CanUnloadTo(const Unit& rPassenger, const Tile& rFrom, const Tile& rTo,
                 const WorldMap& rWorldMap);

// Embark onto FindBoardableTransport. When bRefuelOnAttach, fill Fuel to max (landing path).
// This is the explicit order (L key): it boards wherever boarding is legal, including in a
// base.
bool TryAttachToTransport(Unit& rPassenger, const WorldMap& rWorldMap,
                          bool bRefuelOnAttach = false);

// Boarding applied silently on arrival. Only a passenger that cannot hold the tile by
// itself is loaded — walking into a base or onto open land never stows a unit behind the
// player's back, and a unit standing in a base stays a garrison rather than becoming cargo.
bool TryAutoAttachOnEntry(Unit& rPassenger, const WorldMap& rWorldMap);

// Landing / stranded air: attach with refuel when a boardable carrier is present.
bool TryAutoAttachWhenMustLand(Unit& rPassenger, const WorldMap& rWorldMap);

// Whether rPassenger survives its carrier being destroyed on rTile: cargo that can hold the
// tile unaided is set down there, anything else goes down with the carrier.
bool SurvivesCarrierLoss(const Unit& rPassenger, const Tile& rTile);

// Drop all cargo of an air carrier onto its current tile when every passenger can hold it
// unaided.
bool CanUnloadTransportInPlace(const Unit& rCarrier);
bool TryUnloadTransportInPlace(Unit& rCarrier);

} // namespace ac
