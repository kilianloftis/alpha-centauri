#pragma once

#include "game/base/BaseTypes.h"

namespace ac
{

class Faction;
class Unit;

// Whether rObserver can see rSubject: own and contact-revealed units always; otherwise its tile
// must be visible (or explored, for VisibleInFog) and every concealment detected.
bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject);

// Whether observer knows which faction owns rSubject: its own units, and any unit without Covert.
// TODO: detection — a covert unit's owner becomes known to an observer that detects it.
bool IsOwnerKnownTo(FactionId_t observer, const Unit& rSubject);

} // namespace ac
