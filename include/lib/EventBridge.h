#pragma once

#include "lib/GameEvent.h"
#include "game/faction/base/BaseManager.h"

namespace ac
{

class GameState;
class EventBus;

class EventBridge
{
public:
    EventBridge(GameState& rState, EventBus& rBus);

    // Wire a base's signals to the EventBus (call when base is added to faction)
    void WireBase(BaseManager& rBase);

private:
    EventBus& m_rBus;
};

} // namespace ac
