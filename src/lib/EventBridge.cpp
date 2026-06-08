#include "lib/EventBridge.h"
#include "lib/EventBus.h"
#include "game/GameState.h"
#include "game/faction/Base.h"

namespace ac
{

EventBridge::EventBridge(GameState& rState, EventBus& rBus)
    : m_rBus(rBus)
{
    // Wire turn started signal
    rState.on_turn_started.connect([this](int turn) {
        m_rBus.publish(EvTurnStarted{ turn });
    });

    // TODO: Wire faction signals (on_tech_discovered, on_base_built, on_eliminated)
    // once Faction gains a FactionId and those signals are added.
}

void EventBridge::WireBase(Base& rBase)
{
    // Wire population gained signal
    rBase.on_pop_gained.connect([this](FactionId factionId, int baseId, int newSize) {
        m_rBus.publish(EvBaseGainedPop{ factionId, baseId, newSize });
    });

    // Wire population lost signal
    rBase.on_pop_lost.connect([this](FactionId factionId, int baseId, int newSize) {
        m_rBus.publish(EvBaseLostPop{ factionId, baseId, newSize });
    });
}

} // namespace ac
