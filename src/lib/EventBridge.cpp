#include "lib/EventBridge.h"

EventBridge::EventBridge(GameState& state, EventBus& bus) {
    // Wire every faction's signals to the bus
    for (auto& faction : state.factions()) {
        faction.on_tech_discovered.connect([&bus, &faction](TechId t) {
            bus.publish(EvTechDiscovered{ faction.id(), t });
            });
            faction.on_base_built.connect([&bus, &faction](int base_id) {
                bus.publish(EvBaseBuilt{ faction.id(), base_id });
            });
            faction.on_eliminated.connect([&bus, &faction]() {
                bus.publish(EvFactionElim{ faction.id() });
            });
        }
        // Wire TurnLoop signals
        state.turn_loop().on_turn_started.connect([&bus](int turn) {
            bus.publish(EvTurnStarted{ turn });
        });
    }
}

