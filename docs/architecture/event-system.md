# Event System Architecture

The event system provides a two-layer architecture for communication between engine components and mods.

```mermaid
graph TB
    subgraph "Layer 1: Internal Signals (Engine Only)"
        Signal[Signal&lt;T&gt;<br/>Templated signal/slot]
        FactionSignals[Faction Signals:<br/>on_tech_discovered<br/>on_base_built<br/>on_eliminated]
        TurnSignals[Turn Signals:<br/>on_turn_started]
        DirectCalls[Direct function calls<br/>Zero heap allocation<br/>Type-safe]
    end

    subgraph "Layer 2: EventBus (Mod-Facing)"
        EventBus[EventBus]
        GameEvent[GameEvent<br/>std::variant]
        Handlers[Handler vector<br/>SubscriptionId]
        StableABI[Stable, versionable ABI<br/>for mods]
    end

    subgraph "Bridge Layer"
        EventBridge[EventBridge]
        BridgeLogic[Subscribes to internal signals<br/>Republishes as GameEvent]
    end

    subgraph "Mod Interface"
        Mods[Mods]
        ModHandlers[Mod event handlers<br/>Subscribe to EventBus]
    end

    Signal --> FactionSignals
    Signal --> TurnSignals
    FactionSignals --> DirectCalls
    TurnSignals --> DirectCalls

    EventBridge --> BridgeLogic
    BridgeLogic --> EventBus
    EventBus --> GameEvent
    EventBus --> Handlers
    EventBus --> StableABI

    FactionSignals -.->|wired at startup| EventBridge
    TurnSignals -.->|wired at startup| EventBridge

    Mods --> ModHandlers
    ModHandlers --> EventBus

    style Signal fill:#f9f,stroke:#333,stroke-width:4px
    style EventBus fill:#bbf,stroke:#333,stroke-width:4px
    style EventBridge fill:#fbf,stroke:#333,stroke-width:3px
    style Mods fill:#bfb,stroke:#333,stroke-width:2px
```

## Component Overview

### Signal<T> (Layer 1)
- **Purpose**: Internal engine-only signal/slot mechanism
- **Characteristics**:
  - Templated on event type
  - Zero heap allocation (small-buffer optimization)
  - Direct function calls, no indirection
  - Type-safe at compile time
- **Usage**: Engine modules wire directly to each other at startup
  - Combat resolver connects to faction.on_eliminated for victory checks
  - UI connects to faction.on_tech_discovered for tech popups
- **Performance**: Fast, no string lookups, no variant boxing
- **Mod Access**: None - this is engine-internal only

### EventBus (Layer 2)
- **Purpose**: Mod-facing event bus with stable ABI
- **Characteristics**:
  - Uses tagged std::variant (GameEvent) for type safety
  - Stable, versionable ABI that mods can depend on
  - Subscription-based handler management
  - Synchronous dispatch
- **API**:
  - `Subscribe(Handler)`: Subscribe to all events with a single handler
  - `Subscribe<T>(Handler)`: Subscribe to specific event type only
  - `Unsubscribe(SubscriptionId)`: Remove a subscription
  - `Publish(GameEvent)`: Publish an event to all handlers
- **Reentrancy contract** (part of the mod-facing ABI): a handler may `Subscribe` or
  `Unsubscribe` during dispatch. `Publish` snapshots the subscription ids and re-looks each one
  up before invoking it, so a handler removed earlier in the same dispatch is **not** called, and
  one added during dispatch does **not** see the in-flight event — it sees the next one. Iterating
  the live handler list instead was undefined behaviour: subscribing reallocates the vector and
  unsubscribing erases from it. `Signal::Emit` gives the same guarantee.
- **Mod Access**: Full - this is the primary mod interface for events

### EventBridge
- **Purpose**: Bridges internal `Signal`s to the mod-facing `EventBus`.
- **Wiring**: per base, through `WireBase`, which is idempotent and keyed by object address —
  identity-preserving transfer keeps the same base wired, while a *reconstructed* base (same
  `BaseId_t`, new address) is wired again. `Faction::OnBaseAdded` drives it, so founding, load
  and post-transfer adopt all wire without any caller remembering to.
- **Pattern**: one-way. Mods observe `EventBus`; they never receive internal signals.

#### What is actually bridged

| Event | Source | Wired by |
|---|---|---|
| `EvBaseGainedPop` | `BaseManager::OnPopGained` | `WireBase` |
| `EvBaseLostPop` | `BaseManager::OnPopLost` | `WireBase` |
| `EvDroneRiot` | `BaseManager::OnIsRioting` | `WireBase` |
| `EvTechDiscovered` | `ResearchManager::OnTechDiscovered` | `WireFaction` |
| `EvBaseBuilt` | `Faction::OnBaseAdded` | `WireFaction` |
| `EvTurnStarted` | published directly by the `TurnStart` stage | — (not via the bridge) |

There is deliberately **no** faction-elimination event: factions are never removed from the game,
because a defeated faction's leader can be freed to re-establish it. `EvFactionElim` was declared
and unpublishable, and has been removed rather than left as a promise the rules cannot keep — see
`docs/game-rules-decisions.md`.

`WireFaction` is idempotent per faction object, matching `WireBase`: wiring twice would deliver
every event twice to every mod.

`tests/game/SampleModTests.cpp` is the worked consumer for this table — it subscribes as a mod
would and asserts the events arrive, so an event that stops firing fails the suite.

## Design Rationale

### Turn-stage hooks

The other half of the mod surface. `config/turn_stages.json` declares `pre` / `post` / `replace`
hooks per stage; `HookContext` holds them, and `TurnStageBase` fires them around the stage body
(a callable `replace` hook suppresses the built-in body entirely).

Each hook receives a `HookArgs_t`: the stage id, the `GameState`, and the `Faction` being
processed (null for a global stage, and for the pre/post hooks that fire on stage entry and exit
rather than per faction). Without that argument a config-declared hook had captured nothing and
could therefore observe nothing — the seam existed but could not host a consumer.

**Not yet built:** `Hook_t::scriptPath` **is rejected at load** — `TurnStageConfigParser` throws
on a non-empty value ("script loading is not available; remove the hook or bind a callback in
C++") rather than accepting one it cannot honour. A hook is therefore reachable only from C++
today. The project already embeds a `LuaRuntime`, so what is missing is the scripting API — what
a mod script may call — not the runtime.

### Two-Layer Architecture
- **Layer 1 (Signal<T>)**: Optimized for engine performance
  - No overhead for internal communication
  - Compile-time type safety
  - Zero heap allocation
- **Layer 2 (EventBus)**: Optimized for mod stability
  - Stable ABI across engine versions
  - Runtime type checking via std::variant
  - Single subscription point for mods

### Separation of Concerns
- Engine modules use fast, type-safe signals internally
- Mods use stable, versioned event bus
- EventBridge isolates mods from internal engine changes
- Internal signal changes don't break mod ABI

### Performance Characteristics
- Internal signals: Zero overhead, direct calls
- EventBridge: One allocation per event (variant construction)
- EventBus: O(n) handler dispatch where n = number of subscribers
- Typical use: Few internal signal listeners, few mod subscribers

### Extensibility
- New internal signals: Add to engine modules, wire in EventBridge
- New event types: Add to GameEvent variant, update EventBridge
- New mod handlers: Subscribe via EventBus API
- No ABI breakage for adding new event types (variant is extensible)

## Integration Points

### Engine Module Wiring
At startup, engine modules connect directly to each other:
```cpp
// Example: Combat resolver wiring
combatResolver.OnUnitDestroyed.Connect([](UnitId id) {
    faction.on_unit_lost.Emit(id);
});
```

### EventBridge Initialization
EventBridge is constructed with GameState and EventBus, then wires all signals:
```cpp
EventBridge bridge(gameState, eventBus);
// Automatically subscribes to all internal signals
```

### Mod Subscription
Mods Subscribe to EventBus for events:
```cpp
auto id = eventBus.Subscribe<EvTechDiscovered>(
    [](const EvTechDiscovered& e) {
        // Handle tech discovery
    }
);
```

## Event Flow

### Internal Signal Flow
1. Engine module emits signal: `faction.on_tech_discovered.Emit(techId)`
2. Direct call to all connected slots
3. Slots execute immediately (synchronous)
4. No heap allocation, no variant boxing

### Mod Event Flow
1. Engine module emits internal signal
2. EventBridge slot receives signal
3. EventBridge creates GameEvent variant
4. EventBridge publishes to EventBus
5. EventBus dispatches to all mod handlers
6. Mod handlers execute (synchronous)

## Future Considerations

### Performance Optimization
- Consider async dispatch for mod events if handler count grows
- Profile EventBus dispatch overhead
- Consider event batching for high-frequency events

### Event Filtering
- Add event filtering support to reduce handler calls
- Allow mods to Subscribe to filtered event streams
- Consider event priority levels

### Event History
- Consider event logging/replay for debugging
- Event history for mod debugging tools
- Event replay for testing scenarios
