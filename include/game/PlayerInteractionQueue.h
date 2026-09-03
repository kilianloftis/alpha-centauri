#pragma once

#include "game/PlayerInteraction.h"

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

    // Drops the front item. Throws when empty: completing what was never presented is a
    // presenter bug, and swallowing it would silently drop the next player's prompt.
    void CompleteFront();

    bool Empty() const { return m_queue.empty(); }
    std::size_t Size() const { return m_queue.size(); }

    // True if any queued item is aimed at audienceFactionId.
    bool HasPendingFor(FactionId_t audienceFactionId) const;

private:
    std::deque<QueuedInteraction_t> m_queue;
};

class GameState;

// Queue an interaction addressed to the player faction. No-op when there is no player
// faction (all-AI game). A free function rather than a member because addressing the player
// needs GameState, and rather than a turn-stage helper because the stages that enqueue are
// not all yielding stages — Population warns about a pending riot without yielding itself.
void EnqueueForPlayer(GameState& rGameState, PlayerInteraction_t payload);

} // namespace ac
