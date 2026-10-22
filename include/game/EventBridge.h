#pragma once

#include "lib/GameEvent.h"
#include <unordered_set>

namespace ac
{

class EventBus;
class BaseManager;
class Faction;

class EventBridge
{
public:
    explicit EventBridge(EventBus& rBus);

    // Idempotent, so it can hang off Faction::OnBaseAdded. Keyed by object: a transfer keeps the
    // address, while a rebuilt base reuses its id at a new one.
    void WireBase(BaseManager& rBase);

    // Tech discovery and OnBaseAdded. Idempotent per faction.
    void WireFaction(Faction& rFaction);

private:
    EventBus& m_rBus;
    std::unordered_set<const BaseManager*> m_wiredBases;
    std::unordered_set<const Faction*> m_wiredFactions;
};

} // namespace ac
