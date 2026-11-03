#pragma once

#include "game/diplomacy/TradeItem.h"

#include <optional>

namespace ac
{

// What one gift item does to standing (make_gift, terranx-map.md "Gifts").
struct GiftStanding_t
{
    // The change to the recipient's friction toward the giver.
    int frictionDelta = 0;
    // The giver's gift count toward the recipient afterwards.
    int gifts = 0;
};

// A proposal that only gives: no status asked, nothing demanded.
bool IsGift(const DiplomaticProposal_t& rProposal);

// rItem's standing as a gift from giver to recipient, read before it moves. nullopt for an item
// SMAC gives no standing for.
std::optional<GiftStanding_t> GiftStanding(FactionId_t giver, FactionId_t recipient,
                                           const TradeItem_t& rItem);

} // namespace ac
