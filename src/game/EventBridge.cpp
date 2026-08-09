#include "game/EventBridge.h"
#include "game/Faction.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "lib/EventBus.h"

namespace ac
{

EventBridge::EventBridge(EventBus& rBus)
    : m_rBus(rBus)
{
    // Faction-scoped signals are wired per faction by WireFaction; there is nothing to do at
    // construction. There is deliberately no elimination event — factions are never removed.
}

void EventBridge::WireBase(BaseManager& rBase)
{
    if (!m_wiredBases.insert(&rBase).second)
    {
        return;
    }

    rBase.OnPopGained.Connect([this, &rBase](int newSize) {
        m_rBus.Publish(EvBaseGainedPop{ rBase.GetFactionId(), rBase.GetBaseId(), newSize });
    });

    rBase.OnPopLost.Connect([this, &rBase](int newSize) {
        m_rBus.Publish(EvBaseLostPop{ rBase.GetFactionId(), rBase.GetBaseId(), newSize });
    });

    rBase.OnIsRioting.Connect([this, &rBase]() {
        m_rBus.Publish(EvDroneRiot{ rBase.GetFactionId(), rBase.GetBaseId() });
    });
}

void EventBridge::WireFaction(Faction& rFaction)
{
    if (!m_wiredFactions.insert(&rFaction).second)
    {
        return;
    }

    const FactionId_t factionId = rFaction.GetFactionId();

    rFaction.GetResearch().OnTechDiscovered.Connect([this, factionId](const TechId& rTechId) {
        m_rBus.Publish(EvTechDiscovered{ factionId, rTechId });
    });

    rFaction.OnBaseAdded.Connect([this, factionId](BaseManager& rBase) {
        m_rBus.Publish(EvBaseBuilt{ factionId, rBase.GetBaseId() });
    });
}

} // namespace ac
