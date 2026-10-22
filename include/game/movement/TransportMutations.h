#pragma once

namespace ac
{

class Unit;

// Embark onto FindBoardableTransport. Explicit order (L key): boards wherever boarding is
// legal, including in a base. Refuel is TurnEnd / IsRefuelSite, not attach.
bool TryAttachToTransport(Unit& rPassenger);

// Boards only a passenger that cannot hold the tile itself, so nothing is stowed behind the
// player's back.
bool TryAutoAttachOnEntry(Unit& rPassenger);

// Landing / stranded air: attach when a boardable carrier is present.
bool TryAutoAttachWhenMustLand(Unit& rPassenger);

bool TryUnloadTransportInPlace(Unit& rCarrier);

} // namespace ac
