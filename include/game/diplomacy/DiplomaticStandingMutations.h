#pragma once

#include "game/base/BaseTypes.h"
#include "game/diplomacy/DiplomacyConfig.h"

namespace ac
{

class GameState;

// holder resents against by delta, clamped to friction_max.
void ApplyFriction(GameState& rGameState, FactionId_t holder, FactionId_t against, int delta);

// faction's blemishes after change, clamped to blemishes_max.
void ApplyBlemishes(GameState& rGameState, FactionId_t faction, BlemishChange_t change);

} // namespace ac
