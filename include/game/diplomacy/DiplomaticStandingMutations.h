#pragma once

#include "game/base/BaseTypes.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/TradeItem.h"

namespace ac
{

class GameState;

// holder resents against by delta, clamped to friction_max.
void ApplyFriction(GameState& rGameState, FactionId_t holder, FactionId_t against, int delta);

// faction's blemishes after change, clamped to blemishes_max.
void ApplyBlemishes(GameState& rGameState, FactionId_t faction, BlemishChange_t change);

// giver's gift count toward recipient, raised to at least floor.
void RaiseGifts(GameState& rGameState, FactionId_t giver, FactionId_t recipient, int floor);

// GiftStanding of rItem from giver to recipient, applied; call it before rItem moves.
void ApplyGiftStanding(GameState& rGameState, FactionId_t giver, FactionId_t recipient,
                       const TradeItem_t& rItem);

} // namespace ac
