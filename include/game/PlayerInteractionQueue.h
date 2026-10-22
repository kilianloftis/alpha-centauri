#pragma once

#include "game/PlayerInteraction.h"

#include <algorithm>
#include <cstddef>
#include <deque>

namespace ac
{

// FIFO of player-facing mid-turn interactions. Owned by GameState. Stages Enqueue and Yield;
// the UI presenter presents Front and CompleteFront after domain resolution.
class PlayerInteractionQueue
{
public:
    void Enqueue(QueuedInteraction_t interaction);

    // Null when empty.
    const QueuedInteraction_t* Front() const;

    // Throws when empty: completing what was never presented is a presenter bug.
    void CompleteFront();

    bool Empty() const { return m_queue.empty(); }
    std::size_t Size() const { return m_queue.size(); }

    // True if any queued item is aimed at audienceFactionId.
    bool HasPendingFor(FactionId_t audienceFactionId) const;

    template <typename Pred_t>
    bool AnyOf(Pred_t&& rPred) const
    {
        return std::any_of(m_queue.begin(), m_queue.end(), rPred);
    }

private:
    std::deque<QueuedInteraction_t> m_queue;
};

class GameState;

// Queues an interaction for the player faction; a no-op in an all-AI game.
void EnqueueForPlayer(GameState& rGameState, PlayerInteraction_t payload);

} // namespace ac
