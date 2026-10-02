#pragma once

#include "game/faction/TradeItem.h"

namespace ac
{

class GameState;

// Applies a proposal IsValidProposal accepts: the requested status, then give (proposer to
// recipient), then demand (recipient to proposer). Not a transaction: an item that throws
// leaves the earlier ones applied.
void ApplyProposal(GameState& rGameState, const DiplomaticProposal_t& rProposal);

} // namespace ac
