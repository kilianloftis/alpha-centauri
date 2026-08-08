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

    // Wire a base's signals to the EventBus. Idempotent: a base already wired is a no-op, so
    // this may be connected once per faction to Faction::OnBaseAdded (the single "base
    // introduced" hook — founding, load, and post-transfer adopt all fire it) without
    // double-wiring startup bases that also call this explicitly, and callers never need to
    // remember to call it. Keyed by object, not baseId: identity-preserving transfer keeps the
    // address (so it stays a no-op), while a *reconstructed* base reuses its baseId at a new
    // address and must be wired again — keying by id would silently leave it unwired.
    void WireBase(BaseManager& rBase);

    // Wire a faction's own signals (tech discovery, and base-built via OnBaseAdded). Idempotent
    // per faction object, same reasoning as WireBase.
    void WireFaction(Faction& rFaction);

private:
    EventBus& m_rBus;
    std::unordered_set<const BaseManager*> m_wiredBases;
    std::unordered_set<const Faction*> m_wiredFactions;
};

} // namespace ac
