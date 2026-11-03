#include "game/diplomacy/DiplomaticStandingMutations.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticGiftRules.h"

#include <algorithm>
#include <optional>

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

void RaiseGifts(GameState& rGameState, FactionId_t giver, FactionId_t recipient, int floor)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const int current = rLedger.GetGifts(giver, recipient);
    rLedger.SetGifts(giver, recipient, std::max(current, floor));
}

void ApplyGiftStanding(GameState& rGameState, FactionId_t giver, FactionId_t recipient,
                       const TradeItem_t& rItem)
{
    const std::optional<GiftStanding_t> standing = GiftStanding(giver, recipient, rItem);
    if (!standing)
    {
        return;
    }
    ApplyFriction(rGameState, recipient, giver, standing->frictionDelta);
    rGameState.GetDiplomacyLedger().SetGifts(giver, recipient, standing->gifts);
}

} // namespace ac
