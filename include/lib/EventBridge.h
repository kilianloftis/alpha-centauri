#pragma once

namespace ac
{

class GameState;
class EventBus;

class EventBridge
{
public:
    EventBridge(GameState& rState, EventBus& rBus);
};

} // namespace ac
