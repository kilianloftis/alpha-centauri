#pragma once

namespace ac
{

class Faction;
class TileEffectsContext;
class Unit;

// Whether rObserver can see rSubject as a unit (distinct from tile fog-of-war).
// Own units are always visible. Units in the observer's FactionRevealedUnits (contact
// reveal) are visible. Otherwise the subject's tile must be currently visible, or the
// subject must carry VisibleInFog and the tile must already be explored. Shroud (never
// explored) still hides a VisibleInFog unit. Every active concealment channel on the
// unit must be covered by a matching Detect effect that reaches that tile for the
// observer. Detect sources must carry a stamped ownerFaction at collection
// (territory-owned improvements or unit-projected auras); unattributed Detect never
// pierces. Tile-area Conceal is gated by AppliesForFaction for the subject (terrain
// Conceal stays universal via unset owner). Map markers, tile picking, and attack all
// use this check.
bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject,
                     const TileEffectsContext& rTileEffects);

} // namespace ac
