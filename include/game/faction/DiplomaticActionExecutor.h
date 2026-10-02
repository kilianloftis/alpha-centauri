#pragma once

#include "game/faction/TradeItem.h"
#include <optional>

namespace ac
{

class GameState;

enum class DiplomaticProposeResult_t
{
    Accepted,
    PendingPlayer,
    Rejected,
    Invalid,
    // A proposal already awaits the player. There is one slot, so accepting this one would
    // discard a proposal whose proposer was already told to wait.
    Busy
};

// Routes proposals: an AI recipient answers at once; the player's is held in the one pending
// slot until Accept or Reject. Owned by GameState.
class DiplomaticActionExecutor
{
public:
    DiplomaticActionExecutor() = default;

    DiplomaticProposeResult_t Propose(GameState& rState, const DiplomaticProposal_t& rProposal);

    // Player UI response to a pending inbound offer.
    bool Accept(GameState& rState);
    void Reject();

    const std::optional<DiplomaticProposal_t>& GetPendingProposal() const { return m_pending; }

private:
    bool EvaluateResponse_(const GameState& rState, FactionId_t recipientId) const;

    std::optional<DiplomaticProposal_t> m_pending;
};

} // namespace ac
