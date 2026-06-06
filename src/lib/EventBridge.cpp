#include "lib/EventBridge.h"
#include "lib/EventBus.h"
#include "lib/GameEvent.h"
#include "game/GameState.h"

namespace ac
{

EventBridge::EventBridge(GameState& rState, EventBus& rBus)
{
    // Wire turn started signal
    rState.on_turn_started.connect([&rBus](int turn) {
        rBus.publish(EvTurnStarted{ turn });
    });

    // TODO: Wire faction signals (on_tech_discovered, on_base_built, on_eliminated)
    // once Faction gains a FactionId and those signals are added.
}

} // namespace ac
