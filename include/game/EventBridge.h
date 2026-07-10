#pragma once

#include "lib/GameEvent.h"

namespace ac
{

class EventBus;
class BaseManager;

class EventBridge
{
public:
    explicit EventBridge(EventBus& rBus);

    // Wire a base's signals to the EventBus (call when base is added to faction)
    void WireBase(BaseManager& rBase);

private:
    EventBus& m_rBus;
};

} // namespace ac
