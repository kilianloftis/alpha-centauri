#pragma once

#include "game/diplomacy/TradeItem.h"

namespace ac
{

class GameState;

// Applies a valid proposal: the status, then give, then demand. A gift writes each item's
// standing just before the item moves. Not a transaction: an item that throws leaves the earlier
// ones applied.
void ApplyProposal(GameState& rGameState, const DiplomaticProposal_t& rProposal);

} // namespace ac
