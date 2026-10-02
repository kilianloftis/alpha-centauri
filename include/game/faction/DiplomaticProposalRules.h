#pragma once

#include "game/faction/TradeItem.h"

namespace ac
{

class GameState;

// Whether rProposal may be offered, and later accepted. Proposer and recipient are distinct
// factions that have met; the proposal asks for something; a requested status is StepUp of the
// current one; and on each side the giver can deliver every item, offers no base or tech twice,
// and can afford the side's total credits. Items may be traded under any status.
bool IsValidProposal(const GameState& rGameState, const DiplomaticProposal_t& rProposal);

} // namespace ac
