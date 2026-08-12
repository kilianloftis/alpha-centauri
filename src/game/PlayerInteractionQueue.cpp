#include "game/PlayerInteractionQueue.h"

#include <stdexcept>
#include <utility>

namespace ac
{

void PlayerInteractionQueue::Enqueue(QueuedInteraction_t interaction)
{
    m_queue.push_back(std::move(interaction));
}

const QueuedInteraction_t* PlayerInteractionQueue::Front() const
{
    if (m_queue.empty())
    {
        return nullptr;
    }
    return &m_queue.front();
}

void PlayerInteractionQueue::CompleteFront()
{
    if (m_queue.empty())
    {
        throw std::runtime_error("PlayerInteractionQueue::CompleteFront: queue is empty");
    }
    m_queue.pop_front();
}

bool PlayerInteractionQueue::HasPendingFor(FactionId_t audienceFactionId) const
{
    for (const QueuedInteraction_t& rItem : m_queue)
    {
        if (rItem.audience == audienceFactionId)
        {
            return true;
        }
    }
    return false;
}

} // namespace ac
