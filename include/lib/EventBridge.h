#pragma once

#include "lib/GameEvent.h"

namespace ac
{

class GameState;
class EventBus;
class Base;

class EventBridge
{
public:
    EventBridge(GameState& rState, EventBus& rBus);

    // Wire a base's signals to the EventBus (call when base is added to faction)
    void WireBase(Base& rBase);

private:
    EventBus& m_rBus;
};

} // namespace ac
