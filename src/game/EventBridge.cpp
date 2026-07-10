#include "game/EventBridge.h"
#include "game/faction/base/BaseManager.h"
#include "lib/EventBus.h"

namespace ac
{

EventBridge::EventBridge(EventBus& rBus)
    : m_rBus(rBus)
{
    // TODO: Wire faction signals (on_tech_discovered, on_base_built, on_eliminated)
    // once Faction gains a FactionId and those signals are added.
}

void EventBridge::WireBase(BaseManager& rBase)
{
    rBase.on_pop_gained.connect([this, &rBase](int newSize) {
        m_rBus.publish(EvBaseGainedPop{ rBase.GetFactionId(), rBase.GetBaseId(), newSize });
    });

    rBase.on_pop_lost.connect([this, &rBase](int newSize) {
        m_rBus.publish(EvBaseLostPop{ rBase.GetFactionId(), rBase.GetBaseId(), newSize });
    });
}

} // namespace ac
