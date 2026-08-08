#pragma once

#include "game/faction/TradeItem.h"
#include <optional>

namespace ac
{

class GameState;

enum class DiplomaticProposeResult
{
    Accepted,
    PendingPlayer,
    Rejected,
    Invalid,
    // A proposal already awaits the player. There is one slot, so accepting this one would
    // discard a proposal whose proposer was already told to wait.
    Busy
};

// Mutates diplomacy/economy/research state from proposals. Owned by GameState.
class DiplomaticActionExecutor
{
public:
    DiplomaticActionExecutor() = default;

    DiplomaticProposeResult Propose(GameState& rState, const DiplomaticProposal_t& rProposal);

    // Player UI response to a pending inbound offer.
    bool Accept(GameState& rState);
    void Reject();

    const std::optional<DiplomaticProposal_t>& GetPendingProposal() const { return m_pending; }

private:
    // What one side of a proposal costs its giver in total. Per-item validation against the
    // full treasury lets two items each worth the whole balance both pass.
    struct GiverCost_t
    {
        int credits = 0;
        std::vector<BaseId_t> baseIds;
    };

    bool Validate_(GameState& rState, const DiplomaticProposal_t& rProposal) const;
    bool ValidateItems_(GameState& rState,
                        FactionId_t giverId,
                        FactionId_t receiverId,
                        const std::vector<TradeItem_t>& rItems) const;
    bool ValidateItem_(GameState& rState,
                       FactionId_t giverId,
                       FactionId_t receiverId,
                       const TradeItem_t& rItem) const;
    // Aggregate check over one giver's whole side: total credits against the treasury, and no
    // base offered twice.
    bool ValidateGiverTotals_(GameState& rState,
                              FactionId_t giverId,
                              const std::vector<TradeItem_t>& rItems) const;
    bool EvaluateResponse_(GameState& rState, FactionId_t recipientId) const;
    void Apply_(GameState& rState, const DiplomaticProposal_t& rProposal);
    void ApplyItems_(GameState& rState,
                     FactionId_t giverId,
                     FactionId_t receiverId,
                     const std::vector<TradeItem_t>& rItems);
    void ApplyItem_(GameState& rState,
                    FactionId_t giverId,
                    FactionId_t receiverId,
                    const TradeItem_t& rItem);

    std::optional<DiplomaticProposal_t> m_pending;
};

} // namespace ac
