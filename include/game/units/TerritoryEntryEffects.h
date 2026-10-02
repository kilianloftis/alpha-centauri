#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class GameState;
class Unit;

// The player chose to break the agreement when rUnit's move order stopped at territoryOwner's
// border: declares Vendetta on the owner and resumes the order.
void BreakAgreementAndContinue(GameState& rGameState, Unit& rUnit, FactionId_t territoryOwner);

} // namespace ac
