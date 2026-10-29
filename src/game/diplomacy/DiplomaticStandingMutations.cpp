#include "game/diplomacy/DiplomaticStandingMutations.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyLedger.h"

namespace ac
{

void ApplyFriction(GameState& rGameState, FactionId_t holder, FactionId_t against, int delta)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomacyCalculator& rCalculator = *rGameState.GetGameData().diplomacyCalculator;
    const int current = rLedger.GetFriction(holder, against);
    rLedger.SetFriction(holder, against, rCalculator.NextFriction(current, delta));
}

void ApplyBlemishes(GameState& rGameState, FactionId_t faction, BlemishChange_t change)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomacyCalculator& rCalculator = *rGameState.GetGameData().diplomacyCalculator;
    const int current = rLedger.GetBlemishes(faction);
    const int next = rCalculator.NextBlemishes(current, change.delta, change.floor);
    rLedger.SetBlemishes(faction, next);
}

} // namespace ac
