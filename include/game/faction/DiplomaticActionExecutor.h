#pragma once

#include "game/faction/TradeItem.h"
#include <optional>

namespace ac
{

class GameState;
struct GameDataContext;

enum class DiplomaticProposeResult
{
    Accepted,
    PendingPlayer,
    Rejected,
    Invalid
};

// Mutates diplomacy/economy/research state from proposals. Owned by GameState.
class DiplomaticActionExecutor
{
public:
    DiplomaticActionExecutor() = default;

    // Required for base transfer reconstruct; Engine / tests set this after construction.
    void SetGameDataContext(const GameDataContext& rGameData) { m_pGameData = &rGameData; }

    DiplomaticProposeResult Propose(GameState& rState, const DiplomaticProposal_t& rProposal);

    // Player UI response to a pending inbound offer.
    bool Accept(GameState& rState);
    void Reject();

    const std::optional<DiplomaticProposal_t>& GetPendingProposal() const { return m_pending; }

private:
    bool Validate_(GameState& rState, const DiplomaticProposal_t& rProposal) const;
    bool ValidateItems_(GameState& rState,
                        FactionId_t giverId,
                        FactionId_t receiverId,
                        const std::vector<TradeItem_t>& rItems) const;
    bool ValidateItem_(GameState& rState,
                       FactionId_t giverId,
                       FactionId_t receiverId,
                       const TradeItem_t& rItem) const;
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
    const GameDataContext* m_pGameData = nullptr;
};

} // namespace ac
