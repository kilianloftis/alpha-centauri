#pragma once

#include "game/PauseOnEventsConfig.h"

#include <vector>

namespace ac
{

class IConstructable;

// The pause-on-event gates a completion matches; InteractionPresenter ORs them. Prototype is an
// extra gate on units.
std::vector<PauseOnEventId_t> CollectProductionPauseGates(const IConstructable& rItem,
                                                          bool bPrototype);

} // namespace ac
