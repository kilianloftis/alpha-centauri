#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class GameState;

// Sets Vendetta between a and b, grants mutual known-contact, and when the pair held a Pact
// relocates guest units off each other's territory. The shared path for a Major atrocity's
// universal Vendetta and for TradeDeclareVendetta_t.
void ApplyVendetta(GameState& rGameState, FactionId_t a, FactionId_t b);

} // namespace ac
