#pragma once

#include "game/PauseOnEventsConfig.h"

#include <vector>

namespace ac
{

class IConstructable;

// Map constructable facts (+ whether this completion is a prototype fielding) to the pause-on-
// event gates that apply. Does not consult settings — InteractionPresenter ORs Allows() over
// the returned set. Prototype is an additional gate on units, not a replacement for
// combat / non-combat.
std::vector<PauseOnEventId_t> CollectProductionPauseGates(const IConstructable& rItem,
                                                          bool bPrototype);

} // namespace ac
