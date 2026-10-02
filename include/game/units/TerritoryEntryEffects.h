#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class GameState;
class Unit;

// The player's answer when rUnit's move order stopped at territoryOwner's border. Breaking
// declares Vendetta on the owner and resumes the order; otherwise the order is cancelled.
void ResolveTerritoryEntry(GameState& rGameState, Unit& rUnit, FactionId_t territoryOwner,
                           bool bBreakAgreement);

} // namespace ac
