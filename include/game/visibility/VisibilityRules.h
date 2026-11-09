#pragma once

#include "game/base/BaseTypes.h"

namespace ac
{

class Faction;
class Unit;

// Whether rObserver can see rSubject: own and contact-revealed units always; otherwise its tile
// must be visible (or explored, for VisibleInFog) and every concealment detected.
bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject);

// Whether observer knows which faction owns rSubject: own units, and any unit without Covert.
// Covert is live; design rebind / move refresh first contact when identity or vision changes.
bool IsOwnerKnownTo(FactionId_t observer, const Unit& rSubject);

} // namespace ac
