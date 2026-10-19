#pragma once

namespace ac
{

class Unit;

// Embark onto FindBoardableTransport. Explicit order (L key): boards wherever boarding is
// legal, including in a base. Refuel is TurnEnd / IsRefuelSite, not attach.
bool TryAttachToTransport(Unit& rPassenger);

// Boarding applied silently on arrival. Only a passenger that cannot hold the tile by
// itself is loaded — walking into a base or onto open land never stows a unit behind the
// player's back, and a unit standing in a base stays a garrison rather than becoming cargo.
bool TryAutoAttachOnEntry(Unit& rPassenger);

// Landing / stranded air: attach when a boardable carrier is present.
bool TryAutoAttachWhenMustLand(Unit& rPassenger);

bool TryUnloadTransportInPlace(Unit& rCarrier);

} // namespace ac
